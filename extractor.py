from fastapi import FastAPI, UploadFile, HTTPException, File
from fastapi.middleware.cors import CORSMiddleware
import fitz  # PyMuPDF
import pytesseract
pytesseract.pytesseract.tesseract_cmd = r"C:\Users\HP\AppData\Local\Programs\Tesseract-OCR\tesseract.exe"

from PIL import Image
import io
import traceback
import logging
import re
from pathlib import Path

# Additional imports for more file types
try:
    import docx
    DOCX_AVAILABLE = True
except ImportError:
    DOCX_AVAILABLE = False
    
try:
    import openpyxl
    import pandas as pd
    EXCEL_AVAILABLE = True
except ImportError:
    EXCEL_AVAILABLE = False

try:
    from bs4 import BeautifulSoup
    HTML_AVAILABLE = True
except ImportError:
    HTML_AVAILABLE = False

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

app = FastAPI(
    title="Enhanced Text Extractor",
    description="Extract text from various file formats including PDF, images, Office documents, and more",
    version="2.0.0"
)

# Add CORS middleware
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# Supported file extensions
SUPPORTED_EXTENSIONS = {
    'pdf': 'PDF documents',
    'png': 'PNG images', 'jpg': 'JPEG images', 'jpeg': 'JPEG images', 
    'tiff': 'TIFF images', 'bmp': 'BMP images', 'gif': 'GIF images',
    'txt': 'Plain text files', 'md': 'Markdown files', 'csv': 'CSV files',
    'json': 'JSON files', 'log': 'Log files', 'xml': 'XML files',
    'html': 'HTML files', 'htm': 'HTML files',
    'docx': 'Word documents (if python-docx installed)',
    'xlsx': 'Excel files (if openpyxl installed)',
    'xls': 'Excel files (if pandas installed)'
}

def clean_text(text: str) -> str:
    """Clean and normalize extracted text"""
    if not text:
        return ""
    
    # Remove excessive whitespace
    text = re.sub(r'\s+', ' ', text)
    # Remove control characters except newlines and tabs
    text = re.sub(r'[\x00-\x08\x0B-\x0C\x0E-\x1F\x7F]', '', text)
    # Normalize line endings
    text = text.replace('\r\n', '\n').replace('\r', '\n')
    # Remove excessive blank lines
    text = re.sub(r'\n\s*\n\s*\n', '\n\n', text)
    
    return text.strip()

def extract_from_pdf(content: bytes, filename: str) -> dict:
    """Extract text from PDF files"""
    try:
        pdf = fitz.open(stream=content, filetype="pdf")
        text_parts = []
        metadata = {
            'pages': len(pdf),
            'title': pdf.metadata.get('title', ''),
            'author': pdf.metadata.get('author', ''),
            'subject': pdf.metadata.get('subject', '')
        }
        
        for page_num in range(len(pdf)):
            page = pdf[page_num]
            page_text = page.get_text()
            if page_text.strip():
                text_parts.append(f"--- Page {page_num + 1} ---\n{page_text}")
        
        pdf.close()
        full_text = "\n\n".join(text_parts)
        
        return {
            'text': clean_text(full_text),
            'metadata': metadata,
            'extraction_method': 'PyMuPDF'
        }
    except Exception as e:
        logger.error(f"PDF extraction failed for {filename}: {str(e)}")
        raise HTTPException(status_code=500, detail=f"PDF extraction failed: {str(e)}")

def extract_from_image(content: bytes, filename: str) -> dict:
    """Extract text from image files using OCR"""
    try:
        image = Image.open(io.BytesIO(content))
        
        # Convert to RGB if necessary
        if image.mode != 'RGB':
            image = image.convert('RGB')
        
        # Get image metadata
        metadata = {
            'size': image.size,
            'mode': image.mode,
            'format': image.format or 'Unknown'
        }
        
        # Perform OCR
        text = pytesseract.image_to_string(image, config='--oem 3 --psm 6')
        
        return {
            'text': clean_text(text),
            'metadata': metadata,
            'extraction_method': 'Tesseract OCR'
        }
    except Exception as e:
        logger.error(f"Image OCR failed for {filename}: {str(e)}")
        raise HTTPException(status_code=500, detail=f"Image OCR failed: {str(e)}")

def extract_from_docx(content: bytes, filename: str) -> dict:
    """Extract text from Word documents"""
    if not DOCX_AVAILABLE:
        raise HTTPException(status_code=400, detail="Word document support not available. Install python-docx.")
    
    try:
        doc = docx.Document(io.BytesIO(content))
        text_parts = []
        
        # Extract paragraph text
        for paragraph in doc.paragraphs:
            if paragraph.text.strip():
                text_parts.append(paragraph.text)
        
        # Extract table text
        for table in doc.tables:
            for row in table.rows:
                row_text = []
                for cell in row.cells:
                    if cell.text.strip():
                        row_text.append(cell.text)
                if row_text:
                    text_parts.append(" | ".join(row_text))
        
        # Basic metadata
        core_props = doc.core_properties
        metadata = {
            'author': core_props.author or '',
            'title': core_props.title or '',
            'subject': core_props.subject or '',
            'paragraphs': len(doc.paragraphs),
            'tables': len(doc.tables)
        }
        
        full_text = "\n".join(text_parts)
        
        return {
            'text': clean_text(full_text),
            'metadata': metadata,
            'extraction_method': 'python-docx'
        }
    except Exception as e:
        logger.error(f"DOCX extraction failed for {filename}: {str(e)}")
        raise HTTPException(status_code=500, detail=f"Word document extraction failed: {str(e)}")

def extract_from_excel(content: bytes, filename: str) -> dict:
    """Extract text from Excel files"""
    if not EXCEL_AVAILABLE:
        raise HTTPException(status_code=400, detail="Excel support not available. Install openpyxl and pandas.")
    
    try:
        # Try reading with pandas first
        excel_file = pd.ExcelFile(io.BytesIO(content))
        text_parts = []
        metadata = {'sheets': excel_file.sheet_names}
        
        for sheet_name in excel_file.sheet_names:
            df = pd.read_excel(excel_file, sheet_name=sheet_name)
            
            # Convert DataFrame to text
            sheet_text = f"=== Sheet: {sheet_name} ===\n"
            
            # Add column headers
            if not df.empty:
                headers = " | ".join(str(col) for col in df.columns)
                sheet_text += f"{headers}\n"
                sheet_text += "-" * len(headers) + "\n"
                
                # Add rows
                for _, row in df.iterrows():
                    row_text = " | ".join(str(val) if pd.notna(val) else "" for val in row)
                    if row_text.strip(" | "):  # Only add non-empty rows
                        sheet_text += f"{row_text}\n"
            
            text_parts.append(sheet_text)
        
        full_text = "\n\n".join(text_parts)
        metadata['total_sheets'] = len(excel_file.sheet_names)
        
        return {
            'text': clean_text(full_text),
            'metadata': metadata,
            'extraction_method': 'pandas/openpyxl'
        }
    except Exception as e:
        logger.error(f"Excel extraction failed for {filename}: {str(e)}")
        raise HTTPException(status_code=500, detail=f"Excel extraction failed: {str(e)}")

def extract_from_html(content: bytes, filename: str) -> dict:
    """Extract text from HTML files"""
    if not HTML_AVAILABLE:
        # Fallback to basic HTML parsing
        text = content.decode('utf-8', errors='replace')
        # Remove HTML tags with simple regex
        text = re.sub(r'<[^>]+>', '', text)
        text = re.sub(r'&[^;]+;', ' ', text)  # Remove HTML entities
        return {
            'text': clean_text(text),
            'metadata': {'method': 'regex'},
            'extraction_method': 'Basic HTML parsing'
        }
    
    try:
        soup = BeautifulSoup(content, 'html.parser')
        
        # Remove script and style elements
        for script in soup(["script", "style"]):
            script.decompose()
        
        # Extract title and meta info
        title = soup.find('title')
        title_text = title.get_text() if title else ''
        
        meta_description = soup.find('meta', attrs={'name': 'description'})
        description = meta_description.get('content', '') if meta_description else ''
        
        # Extract main text content
        text = soup.get_text()
        
        metadata = {
            'title': title_text,
            'description': description,
            'method': 'BeautifulSoup'
        }
        
        return {
            'text': clean_text(text),
            'metadata': metadata,
            'extraction_method': 'BeautifulSoup HTML parser'
        }
    except Exception as e:
        logger.error(f"HTML extraction failed for {filename}: {str(e)}")
        raise HTTPException(status_code=500, detail=f"HTML extraction failed: {str(e)}")

def extract_from_text(content: bytes, filename: str) -> dict:
    """Extract text from plain text files with encoding detection"""
    encodings = ['utf-8', 'utf-16', 'latin-1', 'cp1252', 'iso-8859-1']
    
    for encoding in encodings:
        try:
            text = content.decode(encoding)
            metadata = {
                'encoding': encoding,
                'size_bytes': len(content),
                'lines': len(text.splitlines())
            }
            
            return {
                'text': clean_text(text),
                'metadata': metadata,
                'extraction_method': f'Text decoding ({encoding})'
            }
        except UnicodeDecodeError:
            continue
    
    # If all encodings fail, use utf-8 with error handling
    text = content.decode('utf-8', errors='replace')
    metadata = {
        'encoding': 'utf-8 (with errors replaced)',
        'size_bytes': len(content),
        'lines': len(text.splitlines())
    }
    
    return {
        'text': clean_text(text),
        'metadata': metadata,
        'extraction_method': 'Text decoding (utf-8 with error handling)'
    }

@app.get("/")
async def root():
    return {
        "service": "Enhanced Text Extractor",
        "version": "2.0.0",
        "supported_formats": SUPPORTED_EXTENSIONS,
        "features": [
            "PDF text extraction",
            "OCR for images",
            "Word document support (if installed)",
            "Excel file support (if installed)", 
            "HTML content extraction",
            "Multi-encoding text file support",
            "Metadata extraction",
            "Text cleaning and normalization"
        ]
    }

@app.get("/health")
async def health_check():
    return {
        "status": "ok", 
        "service": "enhanced-text-extractor",
        "dependencies": {
            "docx_support": DOCX_AVAILABLE,
            "excel_support": EXCEL_AVAILABLE,
            "html_support": HTML_AVAILABLE
        }
    }

@app.get("/formats")
async def supported_formats():
    """Return detailed information about supported file formats"""
    return {
        "supported_extensions": SUPPORTED_EXTENSIONS,
        "optional_dependencies": {
            "python-docx": {"installed": DOCX_AVAILABLE, "enables": "Word document (.docx) support"},
            "openpyxl + pandas": {"installed": EXCEL_AVAILABLE, "enables": "Excel file (.xlsx, .xls) support"},
            "beautifulsoup4": {"installed": HTML_AVAILABLE, "enables": "Advanced HTML parsing"}
        },
        "always_available": [
            "PDF (.pdf)",
            "Images (.png, .jpg, .jpeg, .tiff, .bmp, .gif)",
            "Text files (.txt, .md, .csv, .json, .log, .xml, .html)"
        ]
    }

@app.post("/extract")
async def extract_text(file: UploadFile = File(...)):
    """Extract text from uploaded file"""
    try:
        # Validation
        if not file.filename:
            raise HTTPException(status_code=400, detail="No filename provided")
        
        content = await file.read()
        if not content:
            raise HTTPException(status_code=400, detail="Empty file provided")
        
        filename = file.filename.lower()
        file_extension = Path(filename).suffix.lower().lstrip('.')
        
        logger.info(f"Processing file: {file.filename} (type: {file.content_type}, size: {len(content)} bytes)")
        
        # Route to appropriate extractor based on file extension
        if file_extension == 'pdf':
            result = extract_from_pdf(content, file.filename)
        elif file_extension in ['png', 'jpg', 'jpeg', 'tiff', 'bmp', 'gif']:
            result = extract_from_image(content, file.filename)
        elif file_extension == 'docx':
            result = extract_from_docx(content, file.filename)
        elif file_extension in ['xlsx', 'xls']:
            result = extract_from_excel(content, file.filename)
        elif file_extension in ['html', 'htm']:
            result = extract_from_html(content, file.filename)
        elif file_extension in ['txt', 'md', 'csv', 'json', 'log', 'xml']:
            result = extract_from_text(content, file.filename)
        else:
            # Try as text file for unknown extensions
            try:
                result = extract_from_text(content, file.filename)
                result['warning'] = f"Unknown file extension '.{file_extension}', treated as text file"
            except Exception:
                raise HTTPException(
                    status_code=400, 
                    detail=f"Unsupported file type: .{file_extension}. Supported formats: {', '.join(SUPPORTED_EXTENSIONS.keys())}"
                )

        # Validate extraction result
        extracted_text = result.get('text', '')
        if not extracted_text.strip():
            return {
                "text": "",
                "filename": file.filename,
                "content_type": file.content_type,
                "size_bytes": len(content),
                "extracted_chars": 0,
                "extraction_method": result.get('extraction_method', 'Unknown'),
                "metadata": result.get('metadata', {}),
                "warning": "No text content found in file"
            }

        # Return successful result
        response = {
            "text": extracted_text,
            "filename": file.filename,
            "content_type": file.content_type,
            "size_bytes": len(content),
            "extracted_chars": len(extracted_text),
            "extraction_method": result.get('extraction_method', 'Unknown'),
            "metadata": result.get('metadata', {}),
            "success": True
        }
        
        if 'warning' in result:
            response['warning'] = result['warning']
            
        logger.info(f"Successfully extracted {len(extracted_text)} characters from {file.filename}")
        return response

    except HTTPException:
        raise
    except Exception as e:
        logger.error(f"Unexpected error processing {file.filename}: {traceback.format_exc()}")
        raise HTTPException(
            status_code=500, 
            detail={
                "error": "Unexpected error during extraction",
                "details": str(e),
                "filename": file.filename if hasattr(file, 'filename') else 'unknown'
            }
        )

@app.post("/extract/batch")
async def extract_batch(files: list[UploadFile] = File(...)):
    """Extract text from multiple files"""
    if len(files) > 10:  # Limit batch size
        raise HTTPException(status_code=400, detail="Batch size limited to 10 files")
    
    results = []
    for file in files:
        try:
            # Process each file individually
            result = await extract_text(file)
            result['status'] = 'success'
            results.append(result)
        except HTTPException as e:
            results.append({
                'filename': file.filename if hasattr(file, 'filename') else 'unknown',
                'status': 'error',
                'error': e.detail,
                'success': False
            })
        except Exception as e:
            results.append({
                'filename': file.filename if hasattr(file, 'filename') else 'unknown', 
                'status': 'error',
                'error': f"Unexpected error: {str(e)}",
                'success': False
            })
    
    return {
        'batch_results': results,
        'total_files': len(files),
        'successful': len([r for r in results if r.get('status') == 'success']),
        'failed': len([r for r in results if r.get('status') == 'error'])
    }

if __name__ == "__main__":
    import uvicorn
    
    print("=" * 60)
    print("🚀 Enhanced Text Extractor Service Starting...")
    print("=" * 60)
    print(f"📄 Supported formats: {len(SUPPORTED_EXTENSIONS)}")
    for ext, desc in SUPPORTED_EXTENSIONS.items():
        print(f"   • .{ext:<6} - {desc}")
    print()
    print("🔧 Optional dependencies:")
    print(f"   • Word documents: {'✅' if DOCX_AVAILABLE else '❌'} (python-docx)")
    print(f"   • Excel files:    {'✅' if EXCEL_AVAILABLE else '❌'} (openpyxl + pandas)")  
    print(f"   • HTML parsing:   {'✅' if HTML_AVAILABLE else '❌'} (beautifulsoup4)")
    print()
    print("🌐 Starting server on http://0.0.0.0:5000")
    print("📡 API documentation: http://localhost:5000/docs")
    print("=" * 60)
    
    uvicorn.run(app, host="0.0.0.0", port=5000, log_level="info")