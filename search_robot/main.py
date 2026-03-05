import sys
import os
import logging
from crawler import Crawler
from database import DatabaseManager


def main():
    if len(sys.argv) < 2:
        print("Использование:")
        print("  python src/main.py crawl <путь_к_config.yaml>")
        print("  python src/main.py stats <путь_к_config.yaml>")
        sys.exit(1)
    
    command = sys.argv[1].lower()
    
    if len(sys.argv) > 2:
        config_path = sys.argv[2]
    else:
        config_path = "config/config.yaml"
    

    if command == "crawl":
        crawler = Crawler(config_path)
        crawler.run()
        
    elif command == "stats":
        db = DatabaseManager(config_path)
        db.print_statistics()
        db.close()
        
    else:
        print(f"Неизвестная команда: {command}")
        print("Доступные команды: crawl, stats")
        sys.exit(1)

if __name__ == "__main__":
    main()