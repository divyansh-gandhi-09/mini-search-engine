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
WORD_POOL = [w.lower() for w in all_words if w.isalpha() and len(w) <= 12]

if not WORD_POOL:
    raise ValueError("Word pool is empty after filtering. Check your filters!")

print(f"Word pool size: {len(WORD_POOL)}")

def make_document(words_per_doc):
    """Generate a pseudo-document with sentences and paragraphs."""
    doc_words = random.sample(WORD_POOL, words_per_doc)
    random.shuffle(doc_words)

    sentences = []
    idx = 0
    while idx < words_per_doc:
        # Random sentence length between 8 and 15 words
        sentence_len = random.randint(8, 15)
        sentence_words = doc_words[idx:idx + sentence_len]
        if not sentence_words:
            break
        # Capitalize first word and add punctuation
        sentence = " ".join(sentence_words)
        sentence = sentence.capitalize() + random.choice([".", ".", ".", "!", "?"])
        sentences.append(sentence)
        idx += sentence_len

    # Group sentences into paragraphs (5–8 sentences each)
    paragraphs = []
    for i in range(0, len(sentences), random.randint(5, 8)):
        para = " ".join(sentences[i:i + random.randint(5, 8)])
        paragraphs.append(para)

    # Join paragraphs with blank lines between them
    return "\n\n".join(paragraphs)


def generate_docs(n, words_per_doc):
    os.makedirs(OUT_DIR, exist_ok=True)

    for i in range(1, n + 1):
        filename = os.path.join(OUT_DIR, f"test_doc_{i}.txt")

        if words_per_doc > len(WORD_POOL):
            raise ValueError(f"WORDS_PER_DOC ({words_per_doc}) exceeds word pool size ({len(WORD_POOL)})")

        text = make_document(words_per_doc)

        with open(filename, "w", encoding="utf-8") as f:
            f.write(text)

        print("Created:", filename)

    print(f"\n✅ {n} test documents generated in '{OUT_DIR}/' with {words_per_doc} words each, formatted as readable text.")


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
