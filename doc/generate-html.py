#!/usr/bin/python3
# Generate a HTML page from the .rst doc files

import docutils.core
import os

files = sorted(os.listdir())
links = []

if not os.path.exists('html'):
    os.mkdir('html')

for doc in files:
    if not doc.endswith('.rst'):
        continue

    basename = doc.rsplit('.', 1)[0]

    docutils.core.publish_file(
        source_path=f'{basename}.rst',
        destination_path=f'html/{basename}.html',
        writer_name='html'
    )

    links.append(f'<a href="html/{basename}.html">{doc}</a><br>')


links = '\n'.join(links)
index_source = f"""
<html>
    <head>
        <title>Coffee Documentation</title>
        <style>

        a {{
            font-family: monospace;
        }}

        </style>
    </head>
    <h1>Coffee Documentation</h1>
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
