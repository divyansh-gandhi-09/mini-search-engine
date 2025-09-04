import os
import random
import argparse
OUT_DIR = "data"
NUM_DOCS = 100 
WORDS_PER_DOC = 100 

WORDS = [
    "search", "engine", "index", "document", "text",
    "query", "data", "system", "information", "retrieval"
]

def generate_docs(n, words_per_doc):
    os.makedirs(OUT_DIR, exist_ok=True)
    for i in range(1, n + 1):
        filename = os.path.join(OUT_DIR, f"test_doc_{i}.txt")
        with open(filename, "w", encoding="utf-8") as f:
            words = [random.choice(WORDS) for _ in range(words_per_doc)]
            f.write(" ".join(words))
        print("Created:", filename)
    print(f"\n✅ {n} test documents generated in '{OUT_DIR}/' with ~{words_per_doc} words each.")

def cleanup_docs():
    removed = 0
    for filename in os.listdir(OUT_DIR):
        if filename.startswith("test_doc_") and filename.endswith(".txt"):
            os.remove(os.path.join(OUT_DIR, filename))
            print("Deleted:", filename)
            removed += 1
    if removed == 0:
        print("⚠️ No test documents found to delete.")
    else:
        print(f"\n✅ Deleted {removed} test documents from '{OUT_DIR}/'")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Generate or clean up test documents.")
    parser.add_argument("--clean", action="store_true",
                        help="Delete generated test documents instead of creating new ones.")
    parser.add_argument("--n", type=int, default=NUM_DOCS,
                        help="Number of documents to generate (default: 100)")
    parser.add_argument("--words", type=int, default=WORDS_PER_DOC,
                        help="Number of words per document (default: 100)")

    args = parser.parse_args()

    if args.clean:
        cleanup_docs()
    else:
        generate_docs(args.n, args.words)
