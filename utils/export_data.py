import yaml
from pymongo import MongoClient
import argparse
import os

def export_data(output_dir):
    host = 'localhost'
    port = 27017
    database_name = 'inf_search'
    collection_name = 'documents'

    print(f"Connecting to MongoDB: {host}:{port}, DB: {database_name}, Collection: {collection_name}")

    client = MongoClient(
        host=host,
        port=port,
        serverSelectionTimeoutMS=5000
    )

    try:
        client.admin.command('ping')
        print("Connection to MongoDB successful.")
    except Exception as e:
        print(f"Error connecting to MongoDB: {e}")
        client.close()
        return False

    db = client[database_name]
    collection = db[collection_name]

    text_filename = os.path.join(output_dir, "text.txt")
    url_filename = os.path.join(output_dir, "url.txt")

    with open(text_filename, 'w', encoding='utf-8') as f_text, \
         open(url_filename, 'w', encoding='utf-8') as f_url:

        cursor = collection.find({}, {'url': 1, 'clean_text': 1, '_id': 0})
        count = 0
        for doc in cursor:
            if 'url' in doc and 'clean_text' in doc:
                clean_text_single_line = doc['clean_text'].replace('\n', ' ').replace('\r', ' ')
                clean_text_single_line = ' '.join(clean_text_single_line.split())

                
                f_text.write(clean_text_single_line + '\n')
                f_url.write(doc['url'] + '\n')
                count += 1
                if count % 5000 == 0:
                    print(f"  Processed {count} documents...")

    print(f"Total documents exported: {count}")

    client.close()
    print("Export finished.")


def main():
    parser = argparse.ArgumentParser()

    parser.add_argument('--output_dir', type=str, default='../texts')

    args = parser.parse_args()


    if not os.path.exists(args.output_dir):
        os.makedirs(args.output_dir)

    export_data(args.output_dir)


if __name__ == '__main__':
    main()