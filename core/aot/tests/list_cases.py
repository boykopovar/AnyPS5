import ast
import sys
from pathlib import Path


for node in ast.parse(Path(sys.argv[1]).read_text()).body:
    if isinstance(node, ast.ClassDef):
        for method in node.body:
            if isinstance(method, ast.FunctionDef) and method.name.startswith('test_'):
                print(f'{node.name}.{method.name}')
