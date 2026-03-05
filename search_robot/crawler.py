from datetime import datetime
import requests
import time
import yaml
from urllib.parse import urlparse, urljoin, parse_qs, urlencode
import signal
import sys
import json
from database import DatabaseManager
from bs4 import BeautifulSoup
from parser import get_parser_by_name


class Crawler:
    def __init__(self, config_path):
        with open(config_path, 'r') as f:
            self.config = yaml.safe_load(f)
        
        self.db = DatabaseManager(config_path)
        self.delay = self.config['logic']['delay_between_requests']
        self.user_agent = self.config['logic']['user_agent']
        self.timeout = self.config['logic']['request_timeout']
        
        self.session = requests.Session()
        self.session.headers.update({
            'User-Agent': self.user_agent,
            'Accept': 'text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8',
            'Accept-Language': 'en-US,en;q=0.5',
            'Accept-Encoding': 'gzip, deflate',
            'Connection': 'keep-alive',
        })
        
        self.running = True
        self.processed_urls = set()

        signal.signal(signal.SIGINT, self.signal_handler)
        signal.signal(signal.SIGTERM, self.signal_handler)
    
    def signal_handler(self, signum, frame):
        """Обработчик сигналов для корректного завершения"""
        print(f"\nПолучен сигнал {signum}. Завершаем работу...")
        self.running = False
    
    def fetch_page(self, url):
        """Загрузка страницы с обработкой ошибок"""
        try:
            print(f"Загружаем: {url}")
            response = self.session.get(url, timeout=self.timeout)
            response.raise_for_status()

            if response.encoding is None:
                response.encoding = 'utf-8'
            
            return response.text
        except requests.exceptions.Timeout:
            print(f"Таймаут при загрузке {url}")
            return None
        except requests.exceptions.HTTPError as e:
            print(f"HTTP ошибка {e.response.status_code} для {url}")
            return None
        except requests.exceptions.RequestException as e:
            print(f"Ошибка при загрузке {url}: {e}")
            return None
    
    def extract_drug_links_from_listing(self, html_content, base_url):
        """Извлечение ссылок на препараты из списка"""
        try:
            soup = BeautifulSoup(html_content, 'html.parser')
            drug_links = []

            for link in soup.find_all('a', href=True):
                href = link['href']
                if '/drugs/' in href and href.count('/') >= 2:
                    if href in ['/drugs', '/drugs/', '/drugs?']:
                        continue
                    
                    full_url = urljoin(base_url, href).split('#')[0].rstrip('/')
                    if '/drugs/' in full_url and full_url != base_url + '/drugs':
                        if full_url not in drug_links:
                            drug_links.append(full_url)
            
            return list(set(drug_links))
        except Exception as e:
            print(f"Ошибка при извлечении ссылок: {e}")
            return []
    
    def discover_and_process_batch(self, source_name, source_config, batch_size=200):
        """Обнаружение и обработка пачки страниц"""
        processed_count = 0
        base_url = source_config['base_url']
        start_url = source_config['start_url']
        parser_name = source_config['parser']
        
        print(f"Начинаем обход с: {start_url}")

        state = self.db.load_crawler_state(source_name)

        existing_count = self.db.collection.count_documents({
            'source_name': source_name,
            'crawled_at': {'$exists': True}
        })
        
        if state:
            current_page = state.get('current_page', 1)
            state_processed_count = state.get('processed_count', 0)
            last_drug_url = state.get('current_url')
            
            print(f"Восстановлено состояние из БД:")
            print(f"  - Страница: {current_page}")
            print(f"  - Обработано в состоянии: {state_processed_count}")
            print(f"  - Уже в БД: {existing_count}")

            total_drugs_processed = max(state_processed_count, existing_count)

            if last_drug_url and total_drugs_processed > 0:
                existing_doc = self.db.collection.find_one({'url': last_drug_url})
                if existing_doc:
                    print(f"Последний препарат '{last_drug_url}' найден в БД")
                    resume_from_last = True
                else:
                    print(f"Последний препарат не найден в БД, начинаем с начала страницы")
                    resume_from_last = False
            else:
                resume_from_last = False
                print("Начинаем с начала страницы")
        else:
            current_page = 1
            total_drugs_processed = existing_count
            resume_from_last = False
            print(f"Состояние не найдено, начинаем с начала. В БД: {existing_count}")
        
        max_pages = 154
        max_drugs = source_config.get('max_pages', 30000)
 
        parser_func = get_parser_by_name(parser_name)
        if not parser_func:
            print(f"Парсер {parser_name} не найден!")
            return 0
        
        while self.running and current_page <= max_pages and total_drugs_processed < max_drugs:
            listing_url = f"{base_url}/drugs?page={current_page}&pageSize=200"
            print(f"\n[Страница {current_page}/{max_pages}] Обработка: {listing_url}")
            print(f"Уже обработано препаратов: {total_drugs_processed}/{max_drugs}")

            html_content = self.fetch_page(listing_url)
            if not html_content:
                print(f"Не удалось загрузить страницу {current_page}")
                current_page += 1
                time.sleep(self.delay)
                continue
 
            drug_links = self.extract_drug_links_from_listing(html_content, base_url)
            print(f"Найдено ссылок на препаратов на странице: {len(drug_links)}")

            start_index = 0
            if resume_from_last and state and 'current_url' in state:
                last_url = state['current_url']
                if last_url in drug_links:
                    last_index = drug_links.index(last_url)
                    start_index = last_index + 1
                    print(f"Начинаем с препарата {start_index + 1} на странице")
                else:
                    print(f"Последний препарат не найден на этой странице, начинаем с начала")
                    resume_from_last = False
            
            drugs_processed_on_page = 0
            for drug_index in range(start_index, len(drug_links)):
                if not self.running or total_drugs_processed >= max_drugs:
                    break
                
                drug_url = drug_links[drug_index]

                if drug_url in self.processed_urls:
                    print(f"[Пропуск] Препарат уже обработан в этой сессии: {drug_url}")
                    continue
                
                existing_doc = self.db.collection.find_one({'url': drug_url})
                if existing_doc and existing_doc.get('crawled_at'):
                    needs_recheck = False
                    if self.db.recheck_days > 0:
                        current_time = int(datetime.utcnow().timestamp())
                        revisit_threshold = current_time - (self.db.recheck_days * 24 * 60 * 60)
                        if existing_doc.get('crawled_at', 0) <= revisit_threshold:
                            needs_recheck = True
                    
                    if not needs_recheck:
                        print(f"[Уже обработан] Пропускаем: {drug_url}")
                        continue
                    else:
                        print(f"[Требует перепроверки] Обрабатываем: {drug_url}")

                drug_html = self.fetch_page(drug_url)
                if not drug_html:
                    print(f"Не удалось загрузить препарат: {drug_url}")
                    time.sleep(self.delay)
                    continue

                clean_text, extracted_urls = parser_func(drug_html, drug_url, base_url)

                saved, status = self.db.save_document(
                    drug_url, 
                    drug_html, 
                    clean_text, 
                    source_name
                )
                
                if saved:
                    if status == "created":
                        print(f"[Создан] {drug_url}")
                    elif status == "updated":
                        print(f"[Обновлен] {drug_url}")
                    elif status == "no_changes":
                        print(f"[Без изменений] {drug_url}")
                    
                    self.processed_urls.add(drug_url)
                    total_drugs_processed += 1
                    drugs_processed_on_page += 1
                    processed_count += 1

                    self.db.save_crawler_state(
                        source_name,
                        drug_url,
                        total_drugs_processed,
                        current_page
                    )

                    if total_drugs_processed % 10 == 0:
                        print(f"Прогресс: {total_drugs_processed}/{max_drugs} препаратов")
                else:
                    print(f"[Ошибка] Не удалось сохранить {drug_url}: {status}")

                time.sleep(self.delay)

            if resume_from_last:
                resume_from_last = False

            current_page += 1

            if self.running and total_drugs_processed < max_drugs:
                self.db.save_crawler_state(
                    source_name,
                    None,
                    total_drugs_processed,
                    current_page
                )
 
            if current_page <= max_pages and self.running:
                time.sleep(self.delay)
        
        if total_drugs_processed >= max_drugs:
            print(f"\nДостигнут лимит в {max_drugs} препаратов")
        elif current_page > max_pages:
            print(f"\nОбработаны все {max_pages} страниц")
        
        print(f"\nЗавершена обработка источника {source_name}")
        print(f"Всего обработано препаратов: {total_drugs_processed}")

        if current_page > max_pages or total_drugs_processed >= max_drugs:
            self.db.clear_crawler_state(source_name)
        
        return processed_count
    
    def process_rlsnet_sitemap(self, source_name, source_config):
        """Специальная обработка для rlsnet через sitemap.xml"""
        base_url = source_config['base_url']
        start_url = source_config['start_url']
        parser_name = source_config['parser']
        
        print(f"Начинаем обработку через sitemap: {start_url}")
        
        parser_func = get_parser_by_name(parser_name)
        if not parser_func:
            print(f"Парсер {parser_name} не найден!")
            return 0

        sitemap_content = self.fetch_page(start_url)
        if not sitemap_content:
            print(f"Не удалось загрузить главный sitemap: {start_url}")
            return 0
        clean_text, sitemap_urls = parser_func(sitemap_content, start_url, base_url)
        print(f"Найдено вложенных sitemap файлов: {len(sitemap_urls)}")
        
        all_article_urls = []
        processed_count = 0

        for sitemap_url in sitemap_urls:
            if not self.running:
                break
                
            print(f"Обрабатываем sitemap: {sitemap_url}")
            
            sitemap_content = self.fetch_page(sitemap_url)
            if not sitemap_content:
                continue

            _, article_urls = parser_func(sitemap_content, sitemap_url, base_url)
            all_article_urls.extend(article_urls)
            
            time.sleep(self.delay)
        
        print(f"Всего найдено статей: {len(all_article_urls)}")

        state = self.db.load_crawler_state(source_name)
        
        start_index = 0
        if state and 'current_url' in state:
            last_url = state.get('current_url')
            if last_url in all_article_urls:
                start_index = all_article_urls.index(last_url) + 1
                print(f"Восстанавливаем с позиции {start_index + 1}/{len(all_article_urls)}")

        max_articles = source_config.get('max_pages', 30000)
        
        for i in range(start_index, len(all_article_urls)):
            if not self.running or processed_count >= max_articles:
                break
                
            article_url = all_article_urls[i]
            
            if article_url in self.processed_urls:
                print(f"[Пропуск] Статья уже обработана: {article_url}")
                continue

            existing_doc = self.db.collection.find_one({'url': article_url})
            if existing_doc and existing_doc.get('crawled_at'):
                needs_recheck = False
                if self.db.recheck_days > 0:
                    current_time = int(datetime.utcnow().timestamp())
                    revisit_threshold = current_time - (self.db.recheck_days * 24 * 60 * 60)
                    if existing_doc.get('crawled_at', 0) <= revisit_threshold:
                        needs_recheck = True
                
                if not needs_recheck:
                    print(f"[Уже обработано] Пропускаем: {article_url}")
                    continue
            
            print(f"[{i+1}/{len(all_article_urls)}] Обрабатываем: {article_url}")
            
            article_html = self.fetch_page(article_url)
            if not article_html:
                print(f"Не удалось загрузить статью: {article_url}")
                time.sleep(self.delay)
                continue

            clean_text, extracted_urls = parser_func(article_html, article_url, base_url)

            if not clean_text or len(clean_text.strip().split('\n')) <= 1:
                print(f"[Пропуск] Пустая статья: {article_url}")
                time.sleep(self.delay)
                continue
            
            saved, status = self.db.save_document(
                article_url, 
                article_html, 
                clean_text, 
                source_name
            )
            
            if saved:
                if status == "created":
                    print(f"[Создана] {article_url}")
                elif status == "updated":
                    print(f"[Обновлена] {article_url}")
                
                self.processed_urls.add(article_url)
                processed_count += 1

                if processed_count % 10 == 0:
                    self.db.save_crawler_state(
                        source_name,
                        article_url,
                        processed_count
                    )
                    print(f"Прогресс: {processed_count}/{max_articles} статей")
            else:
                print(f"[Ошибка] Не удалось сохранить {article_url}")
            
            time.sleep(self.delay)
        
        print(f"\nЗавершена обработка sitemap для {source_name}")
        print(f"Обработано статей: {processed_count}")
        
        return processed_count
    
    def crawl_source(self, source_name, source_config):
        """Обкачка одного источника"""

        existing_count = self.db.collection.count_documents({
            'source_name': source_name,
            'crawled_at': {'$exists': True}
        })
        
        print(f"\nСтатистика по источнику '{source_name}':")
        print(f"  Уже обработано в БД: {existing_count}")
        
        if existing_count >= source_config['max_pages']:
            print(f"Лимит уже достигнут. Пропускаем источник {source_name}")
            return 0
        
        print(f"\nНачинаем обход источника '{source_name}'...")

        if source_config.get('parser') == 'rlsnet':
            processed_count = self.process_rlsnet_sitemap(source_name, source_config)
        else:
            processed_count = self.discover_and_process_batch(
                source_name, 
                source_config,
                batch_size=200
            )
        
        print(f"\nЗавершена обкачка источника '{source_name}'")
        print(f"Обработано документов в этой сессии: {processed_count}")
        print(f"Всего обработано в БД: {existing_count + processed_count}")
        print(f"{'='*60}")
        
        return processed_count
    
    def run(self):
        """Основной цикл работы робота"""
        start_time = time.time()
        
        try:
            for source_name, source_config in self.config['sources'].items():
                if not self.running:
                    break

                print(f"ИСТОЧНИК: {source_name}")

                processed = self.crawl_source(source_name, source_config)
                
                if not self.running:
                    print(f"\nОстановлено пользователем после источника '{source_name}'")
                    break

                if source_name != list(self.config['sources'].keys())[-1]:
                    time.sleep(10)
        
        except KeyboardInterrupt:
            print("\nОбкачка прервана пользователем")
        except Exception as e:
            print(f"\nОшибка во время обкачки: {e}")
            import traceback
            traceback.print_exc()
        finally:
            total_time = time.time() - start_time
            hours = total_time // 3600
            minutes = (total_time % 3600) // 60
            seconds = total_time % 60
            print(f"Общее время работы: {hours:.0f}ч {minutes:.0f}м {seconds:.0f}с")

            
            self.db.close()
            print("\nРобот завершил работу.")
            print("=" * 60)