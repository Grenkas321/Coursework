import asyncio
from playwright.async_api import async_playwright
import tempfile
import os
import json
import subprocess
import cairosvg

def render_via_pdf_to_png(mermaid_code, output_png, dpi=300):
    """
    1. Рендерим в PDF (векторный формат)
    2. Конвертируем в PNG с высоким DPI
    """
    # Сначала создаем PDF
    pdf_file = tempfile.NamedTemporaryFile(suffix='.pdf', delete=False).name
    
    # Конфиг для Mermaid
    config = {
        "maxEdges": 50000,
        "securityLevel": "loose",
        "theme": "default"
    }
    
    # Создаем временный файл с кодом
    with tempfile.NamedTemporaryFile(mode='w', suffix='.mmd', delete=False) as f:
        f.write(mermaid_code)
        mmd_file = f.name
    
    with tempfile.NamedTemporaryFile(mode='w', suffix='.json', delete=False) as f:
        json.dump(config, f)
        config_file = f.name
    
    try:
        # 1. Создаем PDF через mermaid-cli
        pdf_cmd = [
            'mmdc',
            '-i', mmd_file,
            '-o', pdf_file,
            '-c', config_file,
            '-e', 'pdf',
            '-b', 'white',
            '--quiet'
        ]
        
        # Увеличиваем память для Node.js
        env = os.environ.copy()
        env['NODE_OPTIONS'] = '--max-old-space-size=8192'
        
        result = subprocess.run(pdf_cmd, capture_output=True, text=True, env=env)
        
        if result.returncode != 0:
            print(f"Ошибка создания PDF: {result.stderr[:500]}")
            return False
        
        # 2. Конвертируем PDF в PNG с высоким DPI
        if os.path.exists('/usr/bin/convert') or os.path.exists('/usr/local/bin/convert'):
            # Используем ImageMagick
            convert_cmd = [
                'convert',
                '-density', str(dpi),  # Ключевой параметр!
                '-quality', '100',
                '-colorspace', 'RGB',
                '-background', 'white',
                '-alpha', 'remove',
                '-alpha', 'off',
                pdf_file,
                output_png
            ]
        else:
            # Используем pdftoppm
            convert_cmd = [
                'pdftoppm',
                '-png',
                '-r', str(dpi),  # DPI
                '-aa', 'yes',
                '-aaVector', 'yes',
                pdf_file,
                os.path.splitext(output_png)[0]
            ]
        
        result = subprocess.run(convert_cmd, capture_output=True, text=True)
        
        if result.returncode == 0:
            print(f"✓ PNG создан с DPI={dpi}: {output_png}")
            return True
        else:
            print(f"Ошибка конвертации: {result.stderr}")
            return False
            
    finally:
        # Очистка временных файлов
        for f in [mmd_file, config_file, pdf_file]:
            try:
                if os.path.exists(f):
                    os.unlink(f)
            except:
                pass

def render_highest_quality(mermaid_code, output_file, max_width=4096):
    """
    Автоматический выбор лучшего метода для высокого качества
    """
    '''
    methods = [
        ("PDF+ImageMagick", lambda: render_via_pdf_to_png(mermaid_code, output_file, dpi=2750)),
        ("SVG+Cairo", lambda: render_via_cairo_svg(mermaid_code, output_file, width=max_width)),
        ("HiDPI Playwright", lambda: render_hq_playwright_sync(mermaid_code, output_file, width=max_width, scale=3)),
        ("HiDPI Puppeteer", lambda: HiDPIMermaidRenderer(dpi_scale=3, width=max_width).render(mermaid_code, output_file)),
    ]
    '''
    
    methods = [
        ("PDF+ImageMagick", lambda: render_via_pdf_to_png(mermaid_code, output_file, dpi=2750))
    ]
    
    print("Поиск лучшего метода рендеринга...")
    
    for method_name, method_func in methods:
        print(f"\nПробуем: {method_name}")
        try:
            if method_func():
                # Проверяем качество
                if os.path.exists(output_file):
                    file_size = os.path.getsize(output_file)
                    print(f"  Размер файла: {file_size / 1024:.0f} KB")
                    
                    if file_size > 50 * 1024:  # > 50KB обычно означает хорошее качество
                        print(f"✓ {method_name} успешен!")
                        return True
                    else:
                        print(f"  Файл слишком мал, возможно плохое качество")
                        continue
        except Exception as e:
            print(f"  Ошибка: {str(e)[:100]}")
            continue
    
    print("\nВсе методы не сработали, пробуем fallback...")
    
    # Fallback: простой SVG
    return render_simple_svg(mermaid_code, output_file)

def render_simple_svg(mermaid_code, output_file):
    """Простой fallback метод"""
    config = {
        "maxEdges": 50000,
        "securityLevel": "loose"
    }
    
    with tempfile.NamedTemporaryFile(mode='w', suffix='.mmd', delete=False) as f:
        f.write(mermaid_code)
        mmd_file = f.name
    
    try:
        cmd = [
            'mmdc',
            '-i', mmd_file,
            '-o', output_file,
            '-e', 'svg',
            '--quiet'
        ]
        
        result = subprocess.run(cmd, capture_output=True, text=True)
        return result.returncode == 0
    finally:
        os.unlink(mmd_file)


mermaid_diagram = 'graph LR\n'
'''
graph_name = 'gener_sp_1800_nodup_new'
with open('/Users/maxbig/ASVK/coursework/big_sp_graphs_new/' + graph_name + '.txt', "r", encoding="utf-8") as f:
    lines = f.readlines()
'''

graph_name = 'gener_sp_77_no_dups_new'
with open('/Users/maxbig/ASVK/coursework/generated_sp_graphs_new0/gener_sp_77_no_dups_new.txt', "r", encoding="utf-8") as f:
    lines = f.readlines()

lines = lines[1:]
for line in lines:
    line = line.replace(':', '')
    line = line.replace(',', ' ;')
    lst = line.split()
    v1 = lst[0]
    if len(lst) == 2:
        continue
    buf = []
    i = 1
    for elem in lst[1:]:
        if elem == ';':
            for v2 in buf[1:]:
                mermaid_diagram += f'    {v1} -- [{i}:{buf[0]}] --> {v2}\n'
            i += 1
            buf = []
        else:
            buf.append(elem)
    for v2 in buf[1:]:
        mermaid_diagram += f'    {v1} -- [{i}:{buf[0]}] --> {v2}\n'

print(mermaid_diagram)

render_highest_quality(mermaid_diagram, graph_name + '.png', max_width=4096)
