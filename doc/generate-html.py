#!/usr/bin/python3
# Generate a HTML page from the .rst doc files

import docutils.core
import os

additional_style = """
body {
    width: 50%;
    margin: auto;
}
"""


def list_dir_recursive(dir: str) -> list:
    files = os.listdir(dir)
    for f in files:
        if os.path.isdir(f):
            files.extend([f + '/' + x for x in list_dir_recursive(f)])
    return files


files = sorted(list_dir_recursive('.'))
links = []

if not os.path.exists('html'):
    os.mkdir('html')

for doc in files:
    if not doc.endswith('.rst'):
        continue
    if doc.startswith('html'):
        continue

    basename = doc.rsplit('.', 1)[0]
    dest = f'html/{basename}.html'

    dest_dir = os.path.dirname(dest)
    if not os.path.exists(dest_dir):
        os.mkdir(dest_dir)

    print('Generating', dest)

    docutils.core.publish_file(
        source_path=f'{basename}.rst',
        destination_path=dest,
        writer_name='html'
    )

    # Add custom styles
    with open(dest) as f:
        html = f.read()

    tag = '<style type="text/css">'
    html = html.replace(tag, tag + additional_style)

    with open(dest, 'w') as f:
        f.write(html)

    links.append(f'<a href="html/{basename}.html">{doc}</a><br>')


links = '\n'.join(links)
index_source = f"""
<html>
    <head>
        <title>Mocha Documentation</title>
        <style>

        a {{
            font-family: monospace;
        }}

        body {{
            width: 50%;
            margin: auto;
            margin-top: 20px;
        }}

        </style>
    </head>
    <h1>Mocha</h1>
    <p>
        A compiled programming language, using the LLVM toolchain, with a focus
        legible syntax, modularity and compatibility with C. This documentation
        is generated from reStructuredText files into simple HTML files, which
        are just sorted alphabetically so this is very basic. There is an idea
        to upgrade this documentation to something proper, but this is enough
        for now.
    </p>
    <hr>
    <p>
        Here is the list of all generated HTML pages from the .rst files:
    </p>
    <body>
{links}
    </body>
</html>
"""

with open('index.html', 'w') as f:
    f.write(index_source)
