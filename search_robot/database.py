from pymongo import MongoClient, ASCENDING, DESCENDING
from datetime import datetime, timedelta
import yaml
import hashlib
import urllib.parse
from typing import Optional, Tuple, Dict, Any
from pymongo.errors import DuplicateKeyError
import re

class DatabaseManager:
    def __init__(self, config_path: str):
        with open(config_path, 'r') as f:
            config = yaml.safe_load(f)
        
        db_config = config['db']
        self.client = MongoClient(
            host=db_config['host'],
            port=db_config['port'],
            serverSelectionTimeoutMS=5000
        )
        self.db = self.client[db_config['database']]
        self.collection = self.db[db_config['collection']]
        
        self.config = config
        self.recheck_days = config['logic'].get('recheck_interval_days', 7)
        
    
    def _normalize_url(self, url: str, base_url: Optional[str] = None) -> str:
        """Нормализация URL"""
        if base_url and not urllib.parse.urlparse(url).scheme:
            url = urllib.parse.urljoin(base_url, url)

        parsed = urllib.parse.urlparse(url)

        scheme = parsed.scheme.lower()
        netloc = parsed.netloc.lower()

        if scheme == 'http' and netloc.endswith(':80'):
            netloc = netloc[:-3]
        elif scheme == 'https' and netloc.endswith(':443'):
            netloc = netloc[:-4]

        path = parsed.path
        if path:
            path = re.sub(r'/{2,}', '/', path)
            if path != '/' and path.endswith('/'):
                path = path.rstrip('/')
        else:
            path = '/'

        normalized = urllib.parse.urlunparse((
            scheme,
            netloc,
            path,
            parsed.params,
            parsed.query,
            ''
        ))
        
        return normalized
    
    def _calculate_hash(self, content: str) -> str:
        """Вычисление хеша контента"""
        return hashlib.sha256(content.encode('utf-8')).hexdigest()
    
    def add_urls_to_crawl(self, urls: list, source_name: str) -> int:
        """Добавление URL в очередь для обкачки"""
        try:
            added_count = 0
            current_time = int(datetime.utcnow().timestamp())
            
            for url in urls:
                normalized_url = self._normalize_url(url)
                existing = self.collection.find_one(
                    {'url': normalized_url},
                    projection={'_id': 1}
                )
                
                if not existing:
                    result = self.collection.update_one(
                        {'url': normalized_url},
                        {
                            '$setOnInsert': {
                                'url': normalized_url,
                                'source_name': source_name,
                                'created_at': current_time,
                            }
                        },
                        upsert=True
                    )
                    
                    if result.upserted_id:
                        added_count += 1
            
            print(f"Добавлено {added_count} новых URL для источника {source_name}")
            return added_count
            
        except Exception as e:
            print(f"Ошибка при добавлении URL: {e}")
            return 0
    
    def save_document(self, url: str, raw_html: str, clean_text: str, 
                     source_name: str) -> Tuple[bool, str]:
        """
        Сохранение документа в БД с атомарными операциями
        """
        try:
            normalized_url = self._normalize_url(url)
            content_hash = self._calculate_hash(raw_html)
            current_time = int(datetime.utcnow().timestamp())
            recheck_threshold = current_time - (self.recheck_days * 86400)
            result = self.collection.find_one_and_update(
                {'url': normalized_url},
                {
                    '$set': {
                        'raw_html': raw_html,
                        'clean_text': clean_text,
                        'content_hash': content_hash,
                        'crawled_at': current_time,
                        'source_name': source_name
                    },
                    '$setOnInsert': {
                        'created_at': current_time,
                    }
                },
                upsert=True,
                return_document=True
            )

            if result.get('crawled_at', 0) < current_time - 1:
                if result.get('content_hash') == content_hash:
                    if result.get('crawled_at', 0) <= recheck_threshold:
                        self.collection.update_one(
                            {'_id': result['_id']},
                            {'$set': {'crawled_at': current_time}}
                        )
                        return True, "rechecked_no_changes"
                    else:
                        return False, "no_changes_recently_checked"
                else:
                    return True, "updated"
            else:
                return True, "created"
                
        except DuplicateKeyError:
            return self.save_document(url, raw_html, clean_text, source_name)
        except Exception as e:
            print(f"Ошибка при сохранении документа {url}: {e}")
            import traceback
            traceback.print_exc()
            return False, f"error: {str(e)}"
    
    def get_next_url_to_crawl(self, source_name: str) -> Optional[str]:
        """Получение следующего URL для обкачки"""
        try:
            current_time = int(datetime.utcnow().timestamp())
            revisit_threshold = current_time - (self.recheck_days * 86400)
            pipeline = [
                {'$match': {
                    'source_name': source_name,
                    '$or': [
                        {'crawled_at': {'$exists': False}},
                        {'crawled_at': {'$lte': revisit_threshold}}
                    ]
                }},
                {'$addFields': {
                    'priority': {
                        '$cond': {
                            'if': {'$ifNull': ['$crawled_at', False]},
                            'then': 2,  # Для перепроверки
                            'else': 1   # Для первичной обкачки
                        }
                    }
                }},
                {'$sort': {
                    'priority': ASCENDING,
                    'created_at': ASCENDING
                }},
                {'$limit': 1},
                {'$project': {
                    'url': 1,
                    '_id': 0
                }}
            ]
            
            result = list(self.collection.aggregate(pipeline))
            if result:
                return result[0]['url']
            
            return None
            
        except Exception as e:
            print(f"Ошибка при получении следующего URL: {e}")
            import traceback
            traceback.print_exc()
            return None
    
    def save_crawler_state(self, source_name, current_url, processed_count, current_page=None):
        """Сохранение состояния робота"""
        try:
            state = {
                'source_name': source_name,
                'current_url': current_url,
                'processed_count': processed_count,
                'last_updated': datetime.utcnow().timestamp()
            }
            
            if current_page is not None:
                state['current_page'] = current_page

            state_collection = self.db['crawler_state']
            state_collection.update_one(
                {'source_name': source_name},
                {'$set': state},
                upsert=True
            )
            return True
        except Exception as e:
            print(f"Ошибка при сохранении состояния: {e}")
            return False
    
    def load_crawler_state(self, source_name: str) -> Optional[Dict[str, Any]]:
        """Загрузка состояния робота"""
        try:
            state_collection = self.db['crawler_state']
            state = state_collection.find_one({'source_name': source_name})
            return state
            
        except Exception as e:
            print(f"Ошибка при загрузке состояния: {e}")
            return None
    
    def clear_crawler_state(self, source_name: str) -> bool:
        """Очистка состояния робота"""
        try:
            state_collection = self.db['crawler_state']
            state_collection.delete_one({'source_name': source_name})
            return True
            
        except Exception as e:
            print(f"Ошибка при очистке состояния: {e}")
            return False
    
    def get_statistics(self, source_name: Optional[str] = None) -> Dict[str, Any]:
        """Получение статистики"""
        try:
            stats = {}
            match_filter = {'source_name': source_name} if source_name else {}
            stats['total_documents'] = self.collection.count_documents(match_filter)
            stats['never_crawled'] = self.collection.count_documents({
                **match_filter,
                'crawled_at': {'$exists': False}
            })
            
            stats['crawled'] = self.collection.count_documents({
                **match_filter,
                'crawled_at': {'$exists': True}
            })
            current_time = int(datetime.utcnow().timestamp())
            revisit_threshold = current_time - (self.recheck_days * 86400)
            stats['need_recheck'] = self.collection.count_documents({
                **match_filter,
                'crawled_at': {'$exists': True, '$lte': revisit_threshold}
            })

            pipeline = [
                {'$match': match_filter},
                {'$project': {
                    'raw_size': {'$strLenCP': {'$ifNull': ['$raw_html', '']}},
                    'clean_size': {'$strLenCP': {'$ifNull': ['$clean_text', '']}},
                    'has_html': {'$cond': [{'$ifNull': ['$raw_html', False]}, 1, 0]},
                    'has_text': {'$cond': [{'$ifNull': ['$clean_text', False]}, 1, 0]}
                }},
                {'$group': {
                    '_id': None,
                    'total_raw_size': {'$sum': '$raw_size'},
                    'total_clean_size': {'$sum': '$clean_size'},
                    'avg_raw_size': {'$avg': '$raw_size'},
                    'avg_clean_size': {'$avg': '$clean_size'},
                    'docs_with_html': {'$sum': '$has_html'},
                    'docs_with_text': {'$sum': '$has_text'},
                    'count': {'$sum': 1}
                }}
            ]
            
            size_stats = list(self.collection.aggregate(pipeline))
            if size_stats:
                stats.update(size_stats[0])
                del stats['_id']
                if 'count' in stats:
                    del stats['count']
            else:
                stats.update({
                    'total_raw_size': 0,
                    'total_clean_size': 0,
                    'avg_raw_size': 0,
                    'avg_clean_size': 0,
                    'docs_with_html': 0,
                    'docs_with_text': 0
                })

            if not source_name:
                pipeline = [
                    {'$match': {'source_name': {'$exists': True, '$ne': None}}},
                    {'$group': {
                        '_id': '$source_name',
                        'total': {'$sum': 1},
                        'crawled': {'$sum': {'$cond': [
                            {'$ifNull': ['$crawled_at', False]}, 1, 0]}},
                        'need_recheck': {'$sum': {'$cond': [
                            {'$and': [
                                {'$ifNull': ['$crawled_at', False]},
                                {'$lte': ['$crawled_at', revisit_threshold]}
                            ]}, 1, 0]}},
                        'raw_size': {'$sum': {'$strLenCP': {'$ifNull': ['$raw_html', '']}}},
                        'clean_size': {'$sum': {'$strLenCP': {'$ifNull': ['$clean_text', '']}}}
                    }},
                    {'$sort': {'total': DESCENDING}}
                ]
                
                sources_stats = list(self.collection.aggregate(pipeline))
                stats['by_source'] = {}
                for item in sources_stats:
                    if item['_id']:
                        stats['by_source'][item['_id']] = {
                            'total': item.get('total', 0),
                            'crawled': item.get('crawled', 0),
                            'need_recheck': item.get('need_recheck', 0),
                            'raw_size': item.get('raw_size', 0),
                            'clean_size': item.get('clean_size', 0),
                            'avg_raw_size': item.get('raw_size', 0) / max(item.get('total', 1), 1),
                            'avg_clean_size': item.get('clean_size', 0) / max(item.get('total', 1), 1)
                        }
            
            return stats
            
        except Exception as e:
            print(f"Ошибка при получении статистики: {e}")
            import traceback
            traceback.print_exc()
            return {}
    
    def print_statistics(self, source_name: Optional[str] = None):
        """Вывод статистики в консоль"""
        stats = self.get_statistics(source_name)
        
        def format_size(bytes_count):
            if bytes_count is None or bytes_count == 0:
                return "0 B"
            
            for unit in ['B', 'KB', 'MB', 'GB']:
                if bytes_count < 1024.0 or unit == 'GB':
                    return f"{bytes_count:.2f} {unit}"
                bytes_count /= 1024.0
        
        print("\n" + "="*60)
        print("СТАТИСТИКА ПОИСКОВОГО РОБОТА")
        print("="*60)
        
        if source_name:
            print(f"Источник: {source_name}")
        
        print(f"\nВсего документов: {stats.get('total_documents', 0):,}")
        print(f"Обкачано: {stats.get('crawled', 0):,}")
        print(f"Ожидают обкачки: {stats.get('never_crawled', 0):,}")
        print(f"Требуют перепроверки: {stats.get('need_recheck', 0):,}")

        docs_with_html = stats.get('docs_with_html', 0)
        docs_with_text = stats.get('docs_with_text', 0)
        if docs_with_html > 0:
            print(f"Содержат HTML: {docs_with_html:,}")
        if docs_with_text > 0:
            print(f"Содержат текст: {docs_with_text:,}")
        
        print(f"\nСырой HTML: {format_size(stats.get('total_raw_size', 0))}")
        print(f"Чистый текст: {format_size(stats.get('total_clean_size', 0))}")
        
        avg_raw = stats.get('avg_raw_size', 0)
        avg_clean = stats.get('avg_clean_size', 0)
        
        if avg_raw > 0:
            print(f"Средний размер HTML: {format_size(avg_raw)}")
        if avg_clean > 0:
            print(f"Средний размер текста: {format_size(avg_clean)}")

        if 'by_source' in stats and stats['by_source']:
            print("\n" + "="*60)
            print("СТАТИСТИКА ПО ИСТОЧНИКАМ")
            print("="*60)
            
            for source, source_stats in stats['by_source'].items():
                print(f"\n[{source}]")
                print(f"  Всего: {source_stats.get('total', 0):,}")
                print(f"  Обкачано: {source_stats.get('crawled', 0):,} "
                      f"({source_stats.get('crawled', 0)/max(source_stats.get('total', 1), 1)*100:.1f}%)")
                print(f"  Требуют перепроверки: {source_stats.get('need_recheck', 0):,}")
                print(f"  Размер HTML: {format_size(source_stats.get('raw_size', 0))}")
                print(f"  Размер текста: {format_size(source_stats.get('clean_size', 0))}")
    
    def close(self):
        """Закрытие соединения с БД"""
        try:
            self.client.close()
        except Exception as e:
            print(f"Ошибка при закрытии соединения: {e}")

