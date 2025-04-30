import os
import random
import string

from pathlib import Path

BASE_DIR = Path("/home/maksd/course_wrok/test_storage")
GROUPS = ["group1", "group2", "group2/subdir", "group3"]

NUM_FILES_RANGE = (3, 5)
TEXT_PROBABILITY = 0.6  # вероятность сгенерировать текстовый файл


def random_filename(index):
    return f"test_file_{index}_{random.randint(1000, 9999)}.txt"


def generate_text_content(size=512):
    return ''.join(random.choices(string.ascii_letters + string.digits + ' \n', k=size))


def generate_binary_content(size=512):
    return os.urandom(size)


def create_files_in_directory(directory: Path):
    directory.mkdir(parents=True, exist_ok=True)
    num_files = random.randint(*NUM_FILES_RANGE)

    print(f"→ Генерация {num_files} файлов в {directory}")

    for i in range(num_files):
        filename = random_filename(i)
        file_path = directory / filename

        if random.random() < TEXT_PROBABILITY:
            content = generate_text_content(random.randint(256, 1024))
            with open(file_path, 'w') as f:
                f.write(content)
        else:
            content = generate_binary_content(random.randint(256, 1024))
            with open(file_path, 'wb') as f:
                f.write(content)


def main():
    for group in GROUPS:
        create_files_in_directory(BASE_DIR / group)

    print("✔️ Генерация завершена.")


if __name__ == "__main__":
    main()
