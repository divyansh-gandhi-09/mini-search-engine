import os
import random
import argparse
import nltk

# Download the words corpus if not already done
nltk.download('words', quiet=True)
from nltk.corpus import words

OUT_DIR = "data"
NUM_DOCS = 100       # default number of documents
WORDS_PER_DOC = 100  # default words per document

# Get real English words and filter for simplicity
all_words = words.words()
# Filter words: lowercase, length <= 12 to avoid rare/long words
WORD_POOL = [w.lower() for w in all_words if w.isalpha() and len(w) <= 12]

if not WORD_POOL:
    raise ValueError("Word pool is empty after filtering. Check your filters!")

print(f"Word pool size: {len(WORD_POOL)}")

def generate_docs(n, words_per_doc):
    os.makedirs(OUT_DIR, exist_ok=True)
    
    for i in range(1, n + 1):
        filename = os.path.join(OUT_DIR, f"test_doc_{i}.txt")
        
        # Pick unique words for this document
        if words_per_doc > len(WORD_POOL):
            raise ValueError(f"WORDS_PER_DOC ({words_per_doc}) exceeds word pool size ({len(WORD_POOL)})")
        
        doc_words = random.sample(WORD_POOL, words_per_doc)
        random.shuffle(doc_words)  # Shuffle to make order random
        
        with open(filename, "w", encoding="utf-8") as f:
            f.write(" ".join(doc_words))
        
        print("Created:", filename)
    
    print(f"\n✅ {n} test documents generated in '{OUT_DIR}/' with {words_per_doc} unique real words each.")

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
    parser = argparse.ArgumentParser(description="Generate or clean up test documents with real words.")
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
