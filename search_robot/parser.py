from urllib.parse import urljoin, urlparse
import re
from bs4 import BeautifulSoup, Comment
import xml.etree.ElementTree as ET
import html
import json

def normalize_url(url, base_url):
    """Нормализация URL-адреса"""
    parsed = urlparse(url)
    if not parsed.scheme:
        normalized = urljoin(base_url, url).split('#')[0].rstrip('/')
    else:
        normalized = url.split('#')[0].rstrip('/')
    return normalized


def extract_text_without_duplicates(element):
    """
    Извлекает текст из элемента без дублирования.
    Рекурсивно обходит дерево, собирая уникальные текстовые узлы.
    """
    texts = []
    seen_texts = set()
    
    def collect_texts(elem):
        if isinstance(elem, str):
            text = elem.strip()
            if text and text not in seen_texts:
                texts.append(text)
                seen_texts.add(text)
            return

        if elem.name in ['script', 'style', 'button', 'svg', 'img', 'link', 'meta']:
            return

        for child in elem.children:
            collect_texts(child)
    
    collect_texts(element)

    result = []
    current_line = []
    
    for text in texts:
        text = re.sub(r'\s+', ' ', text)
        if text:
            current_line.append(text)
    
    if current_line:
        result.append(' '.join(current_line))
    
    return '\n'.join(result)


def parse_isgeotar(html_content, url, base_url=None):
    """
    Парсер для сайта lsgeotar.ru
    """
    try:
        soup = BeautifulSoup(html_content, 'html.parser')

        title = ""
        title_tag = soup.find('title')
        if title_tag:
            title = title_tag.get_text(strip=True)
            title = title.replace('- Информация о препарате - ЛС ГЭОТАР', '').strip()
            title = title.replace('ЛС ГЭОТАР', '').strip()
            title = re.sub(r'\s*-\s*$', '', title)
        
        clean_parts = []
        if title:
            clean_parts.append(f"{title}")

        for element in soup.find_all(['script', 'style', 'button', 'svg', 'img', 
                                    'link', 'meta', 'form', 'input', 'select']):
            element.decompose()

        test_id_elements = {}
        
        for elem in soup.find_all(attrs={'data-test-id': True}):
            test_id = elem.get('data-test-id')
            if test_id:
                if 'label' in test_id:
                    elem_type = 'label'
                elif any(x in test_id for x in ['content', 'value', 'cms']):
                    elem_type = 'content'
                elif any(x in test_id for x in ['entity-info-item', 'info-item']):
                    if elem.find('h3') or 'label' in elem.get('class', []):
                        elem_type = 'label'
                    else:
                        elem_type = 'unknown'
                else:
                    elem_type = 'unknown'

                if test_id not in test_id_elements:
                    test_id_elements[test_id] = {
                        'element': elem,
                        'type': elem_type,
                        'text': None
                    }

        processed_labels = set()
        
        for test_id, data in test_id_elements.items():
            if data['type'] == 'label':
                label_elem = data['element']

                label_text = label_elem.get_text(strip=True)
 
                for btn in label_elem.find_all(['button', 'svg', 'a']):
                    btn.decompose()
                
                label_text = label_elem.get_text(strip=True)
                label_text = re.sub(r'\s*<!-- -->\s*', '', label_text)
                label_text = re.sub(r'\s+', ' ', label_text).strip()
                
                if not label_text or label_text in processed_labels:
                    continue
                
                processed_labels.add(label_text)

                content_elem = None
                content_text = ""

                parent = label_elem.parent
                if parent:
                    for sibling in parent.find_all(['div', 'span']):
                        sibling_id = sibling.get('data-test-id', '')
                        sibling_class = sibling.get('class', [])
                        
                        if any(x in sibling_id for x in ['content', 'value', 'cms']):
                            content_elem = sibling
                            break
                        elif any('content' in str(c) or 'value' in str(c) or 'cms' in str(c) 
                                 for c in sibling_class):
                            content_elem = sibling
                            break

                if not content_elem:
                    base_match = re.match(r'(.+?)-(label|title|header)', test_id)
                    if base_match:
                        base = base_match.group(1)
                        possible_content_ids = [
                            f"{base}-content",
                            f"{base}-value",
                            f"{base}-cms",
                            test_id.replace('-label', '-content'),
                            test_id.replace('-title', '-content'),
                            test_id.replace('-header', '-content')
                        ]
                        
                        for content_id in possible_content_ids:
                            if content_id in test_id_elements:
                                content_elem = test_id_elements[content_id]['element']
                                break

                if content_elem:
                    content_text = extract_text_without_duplicates(content_elem)
                    content_text = re.sub(r'\s*\n\s*', '\n', content_text)
                    content_text = content_text.strip()
                    lines = content_text.split('\n')
                    unique_lines = []
                    seen_lines = set()
                    
                    for line in lines:
                        line = line.strip()
                        if line and line not in seen_lines:
                            is_duplicate = False
                            for seen in seen_lines:
                                if line in seen or seen in line:
                                    is_duplicate = True
                                    break
                            
                            if not is_duplicate:
                                unique_lines.append(line)
                                seen_lines.add(line)
                    
                    content_text = '\n'.join(unique_lines)
                if not content_text:
                    if parent:
                        all_text = parent.get_text(separator='\n', strip=True)
                        lines = all_text.split('\n')
                        if lines and lines[0].strip() == label_text:
                            lines = lines[1:]
                        content_text = '\n'.join(lines).strip()
                if (content_text and 
                    content_text.lower() not in ['нет данных', 'none', '—', '-', ''] and
                    not re.match(r'^\s*$', content_text)):

                    if '\n' in content_text:
                        clean_parts.append(f"{label_text}:")
                        for line in content_text.split('\n'):
                            if line.strip():
                                clean_parts.append(f"  {line.strip()}")
                    else:
                        clean_parts.append(f"{label_text}: {content_text}")

        if len(clean_parts) <= 1:
            info_patterns = [
                re.compile(r'EntityInfoItem'),
                re.compile(r'info-item'),
                re.compile(r'item-module'),
                re.compile(r'^Item-'),
                re.compile(r'infoItem')
            ]
            
            for pattern in info_patterns:
                info_divs = soup.find_all('div', class_=pattern)
                if info_divs:
                    break
            
            for div in info_divs:
                label_elem = None
                label_selectors = [
                    {'name': 'h3'},
                    {'class': re.compile(r'label|Label|title|Title')},
                    {'class': re.compile(r'text-sm|textSm')},
                    {'class': re.compile(r'basis-1/5|min-w-\[10em\]')}
                ]
                
                for selector in label_selectors:
                    label_elem = div.find(**selector)
                    if label_elem:
                        break
                
                if not label_elem:
                    continue
                label_text = label_elem.get_text(strip=True)
                for btn in label_elem.find_all(['button', 'svg']):
                    btn.decompose()
                
                label_text = label_elem.get_text(strip=True)
                label_text = re.sub(r'\s*<!-- -->\s*', '', label_text).strip()
                
                if not label_text:
                    continue
                content_elem = None
                content_selectors = [
                    {'class': re.compile(r'content|Content|value|Value')},
                    {'class': re.compile(r'basis-4/5|basis4/5')},
                    {'class': re.compile(r'EntityInfoItemValue|infoItemValue')},
                    {'class': re.compile(r'cms|Cms')}
                ]
                
                for selector in content_selectors:
                    content_elem = div.find('div', selector)
                    if content_elem:
                        break
                
                if not content_elem:
                    next_elem = label_elem.find_next_sibling('div')
                    if next_elem:
                        content_elem = next_elem
                
                if content_elem:
                    content_text = extract_text_without_duplicates(content_elem)
                    content_text = content_text.strip()
                    
                    if content_text and content_text.lower() != 'нет данных':
                        lines = content_text.split('\n')
                        unique_lines = []
                        for line in lines:
                            line = line.strip()
                            if line and line not in unique_lines:
                                unique_lines.append(line)
                        
                        content_text = '\n'.join(unique_lines)
                        
                        if content_text:
                            clean_parts.append(f"{label_text}: {content_text}")

        if len(clean_parts) <= 1:
            all_h3 = soup.find_all('h3')
            for h3 in all_h3:
                label_text = h3.get_text(strip=True)
                for btn in h3.find_all(['button', 'svg']):
                    btn.decompose()
                
                label_text = h3.get_text(strip=True)
                label_text = re.sub(r'\s*<!-- -->\s*', '', label_text).strip()
                
                if not label_text or len(label_text) < 2:
                    continue
                content_elem = h3.find_next('div')
                if content_elem:
                    content_text = extract_text_without_duplicates(content_elem)
                    content_text = content_text.strip()
                    
                    if content_text and content_text.lower() != 'нет данных':
                        clean_parts.append(f"{label_text}: {content_text}")
        seen_parts = set()
        unique_parts = []
        for part in clean_parts:
            normalized = re.sub(r'\s+', ' ', part).strip()
            if normalized and normalized not in seen_parts:
                seen_parts.add(normalized)
                unique_parts.append(part)
        
        clean_text = '\n'.join(unique_parts)
        if len(clean_text.split('\n')) < 3:
            main_content = soup.find('main') or soup.find('div', {'class': re.compile(r'container|main|content')})
            if not main_content:
                main_content = soup.find('body')
            
            if main_content:
                additional_text = extract_text_without_duplicates(main_content)
                if additional_text:
                    lines = additional_text.split('\n')
                    filtered_lines = []
                    for line in lines:
                        line = line.strip()
                        if len(line) > 10 and not any(x in line.lower() for x in 
                                                     ['cookie', 'политика', '©', 'copyright', 'rights']):
                            filtered_lines.append(line)
                    
                    if filtered_lines:
                        clean_text += '\n\n' + '\n'.join(filtered_lines[:20])

        extracted_urls = []
        if base_url:
            for link in soup.find_all('a', href=True):
                href = link['href']
                if (href and not href.startswith('#') and 
                    not href.startswith('javascript:') and 
                    not href.startswith('mailto:')):
                    
                    normalized = normalize_url(href, base_url)
                    if normalized and normalized not in extracted_urls:
                        if '/drugs/' in normalized or base_url in normalized:
                            extracted_urls.append(normalized)
        
        return clean_text.strip(), extracted_urls
        
    except Exception as e:
        print(f"Ошибка при парсинге {url}: {e}")
        import traceback
        traceback.print_exc()
        return "", []

def parse_rlsnet_sitemap(sitemap_content, base_url):
    """
    Парсит XML sitemap сайта rlsnet.ru.
    """
    urls = []
    
    try:
        namespaces = {'ns': 'http://www.sitemaps.org/schemas/sitemap/0.9'}
        
        root = ET.fromstring(sitemap_content)
        if 'sitemapindex' in root.tag:

            for sitemap_elem in root.findall('.//ns:sitemap', namespaces):
                loc_elem = sitemap_elem.find('ns:loc', namespaces)
                if loc_elem is not None and loc_elem.text:
                    sitemap_url = normalize_url(loc_elem.text, base_url)
                    urls.append(sitemap_url)
        
        elif 'urlset' in root.tag:
            for url_elem in root.findall('.//ns:url', namespaces):
                loc_elem = url_elem.find('ns:loc', namespaces)
                if loc_elem is not None and loc_elem.text:
                    article_url = normalize_url(loc_elem.text, base_url)
                    urls.append(article_url)
    
    except ET.ParseError as e:
        print(f"Ошибка парсинга XML sitemap: {e}")
    return urls

def parse_rlsnet_article(html_content, url, base_url=None):
    """
    Парсер для статей сайта rlsnet.ru.
    """
    try:
        soup = BeautifulSoup(html_content, 'html.parser')

        if "отсутствует в БД РЛС" in html_content:
            return "", []

        title = ""
        json_ld_content = ""
        script_tags = soup.find_all('script', type='application/ld+json')
        for script in script_tags:
            try:
                script_text = script.string
                if script_text:
                    json_ld_content = script_text
                    headline_match = re.search(r'"headline"\s*:\s*"([^"]+)"', script_text)
                    if headline_match:
                        title = headline_match.group(1).strip()
                        break
            except:
                continue
        
        if not title:
            title_tag = soup.find('title')
            if title_tag:
                title = title_tag.get_text(strip=True)

        clean_parts = []
        if title:
            clean_parts.append(title)

        article_body = ""
        if json_ld_content:
            article_body_match = re.search(r'"articleBody"\s*:\s*"([^"]+)"', json_ld_content)
            if article_body_match:
                article_body = article_body_match.group(1)
                article_body = article_body.replace('\\"', '"')
                article_body = article_body.replace('\\n', ' ')
                article_body = article_body.replace('\\t', ' ')
                article_body = re.sub(r'\s+', ' ', article_body).strip()
        if article_body:
            clean_parts.append(article_body)

        for element in soup.find_all(['script', 'style', 'link', 'meta']):
            element.decompose()

        for element in soup.find_all(class_=re.compile(r'banner|advert|реклама|Reklama|modal|popup', re.I)):
            element.decompose()

        for method_div in soup.find_all(['div', 'section'], class_=re.compile(r'js-banner-group|banner-group|stat-banner')):
            method_div.decompose()

        for nav_element in soup.find_all(['button', 'nav', 'menu', 'header', 'footer', 'aside']):
            nav_element.decompose()

        structure_data = []
        seen_headings = set()
        structure_headings = soup.find_all('h2', class_='structure-heading')
        
        for heading in structure_headings:
            heading_text = heading.get_text(strip=True)

            if heading_text in seen_headings:
                continue
            
            seen_headings.add(heading_text)

            content_text = ""
            content_div = heading.find_next('div')
            if content_div and content_div.find('a'):
                links = content_div.find_all('a')
                link_texts = []
                for link in links:
                    link_text = link.get_text(strip=True)
                    if link_text:
                        link_texts.append(link_text)
                if link_texts:
                    content_text = '; '.join(link_texts)

            if not content_text:
                text_elements = []
                current_elem = heading.next_sibling
                
                while current_elem and (not hasattr(current_elem, 'name') or 
                                       current_elem.name != 'h2'):

                    if isinstance(current_elem, str):
                        text = current_elem.strip()
                        if text:
                            text = re.sub(r'\s+', ' ', text)
                            if text not in ['', ' ', ' ']:
                                if not any(re.search(pattern, text, re.I) for pattern in 
                                          [r'реклама', r'внимание!', r'информация исключительно', 
                                           r'специалист.*здравоохранения', r'описание проверено']):
                                    text_elements.append(text)

                    elif hasattr(current_elem, 'name'):
                        elem_class = current_elem.get('class', [])
                        elem_class_str = ' '.join(elem_class) if elem_class else ''
                        
                        if (current_elem.name not in ['script', 'style', 'h2'] and
                            not any(re.search(pattern, elem_class_str, re.I) for pattern in 
                                   [r'banner', r'advert', r'reklama', r'modal', r'popup'])):
                            
                            elem_text = current_elem.get_text(strip=True)
                            if elem_text:
                                elem_text = re.sub(r'<[^>]+>', '', elem_text)
                                elem_text = re.sub(r'\s+', ' ', elem_text).strip()
                                if not any(re.search(pattern, elem_text, re.I) for pattern in 
                                          [r'реклама', r'внимание!', r'информация исключительно', 
                                           r'специалист.*здравоохранения', r'описание проверено',
                                           r'крылов.*юрий.*федорович', r'опыт работы.*лет']):
                                    text_elements.append(elem_text)
                    if hasattr(current_elem, 'next_sibling'):
                        current_elem = current_elem.next_sibling
                    else:
                        break

                if text_elements:
                    unique_texts = []
                    seen_text_parts = set()
                    
                    for text in text_elements:
                        normalized_text = re.sub(r'[^\w\s]', '', text.lower())
                        normalized_text = re.sub(r'\s+', ' ', normalized_text).strip()

                        words = set(normalized_text.split())

                        is_duplicate = False
                        for seen in seen_text_parts:
                            seen_words = set(seen.split())
                            common_words = words & seen_words
                            if len(common_words) > max(3, len(words) * 0.5):
                                is_duplicate = True
                                break
                        
                        if not is_duplicate and len(text.strip()) > 2:
                            unique_texts.append(text.strip())
                            seen_text_parts.add(normalized_text)
                    
                    if unique_texts:
                        content_text = ' '.join(unique_texts).strip()

            if "Состав и форма выпуск" in heading_text:
                table_div = heading.find_next('div', class_='table-responsive')
                if table_div:
                    table = table_div.find('table')
                    if table:
                        table_data = []
                        rows = table.find_all('tr')
                        for row in rows:
                            cells = row.find_all(['td', 'th'])
                            if len(cells) >= 2:
                                key = cells[0].get_text(strip=True)
                                value = cells[1].get_text(strip=True)
                                if key and value:
                                    table_data.append(f"{key}: {value}")
                        
                        if table_data:
                            content_text = '\n'.join(table_data)

            if content_text:
                content_text = re.sub(r'<[^>]+>', '', content_text)
                content_text = re.sub(r'\s+', ' ', content_text).strip()
                if heading_text in content_text:
                    content_text = content_text.replace(heading_text, '').strip()
                if content_text and not any(re.search(pattern, content_text, re.I) for pattern in 
                                          [r'реклама', r'внимание!', r'информация исключительно', 
                                           r'специалист.*здравоохранения', r'описание проверено',
                                           r'крылов.*юрий.*федорович', r'опыт работы.*лет']):

                    is_duplicate_of_article_body = False
                    if article_body:
                        content_normalized = re.sub(r'[^\w\s]', '', content_text.lower())
                        article_normalized = re.sub(r'[^\w\s]', '', article_body.lower())
                        content_words = set(content_normalized.split())
                        article_words = set(article_normalized.split())

                        common_words = content_words & article_words
                        if len(common_words) > max(5, len(content_words) * 0.7):
                            is_duplicate_of_article_body = True
                    
                    if not is_duplicate_of_article_body:
                        structure_data.append(f"{heading_text}: {content_text}")

        if structure_data:
            unique_structure_data = []
            seen_structure = set()
            
            for item in structure_data:
                normalized_item = re.sub(r'[^\w\s:]', '', item.lower())
                normalized_item = re.sub(r'\s+', ' ', normalized_item).strip()
                
                if normalized_item not in seen_structure:
                    unique_structure_data.append(item)
                    seen_structure.add(normalized_item)

            if unique_structure_data:
                if clean_parts:
                    clean_parts.append("")

                clean_parts.extend(unique_structure_data)

        if clean_parts:
            final_text_parts = []
            seen_texts = set()
            
            for part in clean_parts:
                if not part.strip():
                    continue

                normalized_part = re.sub(r'[^\w\s]', '', part.lower())
                normalized_part = re.sub(r'\s+', ' ', normalized_part).strip()

                part_words = set(normalized_part.split())
                is_duplicate = False
                for seen in seen_texts:
                    seen_words = set(seen.split())
                    common_words = part_words & seen_words
                    if len(common_words) > max(5, len(part_words) * 0.6):
                        is_duplicate = True
                        break
                
                if not is_duplicate and normalized_part:
                    final_text_parts.append(part.strip())
                    seen_texts.add(normalized_part)
            
            clean_parts = final_text_parts

        if len(clean_parts) <= 1:
            return "", []  
        clean_text = '\n'.join(clean_parts)

        lines = clean_text.split('\n')
        if len(lines) > 3:
            last_lines = lines[-3:] if len(lines) >= 3 else lines
            other_lines = lines[:-3] if len(lines) >= 3 else []
            other_text = ' '.join(other_lines).lower()
            other_text = re.sub(r'[^\w\s]', '', other_text)

            filtered_last_lines = []
            for line in last_lines:
                line_normalized = re.sub(r'[^\w\s]', '', line.lower()).strip()
                if line_normalized:
                    line_words = set(line_normalized.split())
                    other_words = set(other_text.split())
                    
                    common_words = line_words & other_words
                    if len(common_words) < max(3, len(line_words) * 0.5):
                        filtered_last_lines.append(line)

            if filtered_last_lines:
                clean_text = '\n'.join(other_lines + filtered_last_lines)
            else:
                clean_text = '\n'.join(other_lines)

        extracted_urls = []
        if base_url:
            for link in soup.find_all('a', href=True):
                href = link['href']
                
                if (not href or href.startswith('#') or 
                    href.startswith('javascript:') or 
                    href.startswith('mailto:')):
                    continue
                
                normalized = normalize_url(href, base_url)
                
                if (normalized and 
                    normalized not in extracted_urls and
                    base_url in normalized):
                    
                    if any(pattern in normalized for pattern in 
                          ['/ads/', '/advert/', '/reklama/', '/banner/', '?utm_', '#']):
                        continue
                    
                    extracted_urls.append(normalized)
        
        return clean_text.strip(), extracted_urls
        
    except Exception as e:
        print(f"Ошибка при парсинге статьи rlsnet {url}: {e}")
        import traceback
        traceback.print_exc()
        return "", []

def parse_rlsnet(html_content, url, base_url=None):
    """
    Универсальный парсер для сайта rlsnet.ru.
    Определяет тип страницы (sitemap или статья) и вызывает соответствующий парсер.
    """
    if url.endswith('.xml') or 'sitemap' in url.lower():
        extracted_urls = parse_rlsnet_sitemap(html_content, base_url or url)
        return f"Sitemap: {url}", extracted_urls
    else:
        return parse_rlsnet_article(html_content, url, base_url)


def get_parser_by_name(parser_name):
    """Возвращает функцию парсера по имени"""
    parsers = {
        'isgeotar': parse_isgeotar,
        'rlsnet': parse_rlsnet
    }
    return parsers.get(parser_name)
