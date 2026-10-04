import os
import glob

def process_file(filepath, page_title):
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()
        
    if '{% extends' in content:
        return
        
    # Extract just the main content (rough extraction)
    if '<main' in content:
        start_idx = content.find('<main')
        end_idx = content.rfind('</main>') + 7
        main_content = content[start_idx:end_idx]
    elif '<body>' in content:
        start_idx = content.find('<body>') + 6
        end_idx = content.rfind('</body>')
        main_content = content[start_idx:end_idx]
    else:
        main_content = content
        
    # Remove old navbars from the content
    import re
    main_content = re.sub(r'<nav.*?</nav>', '', main_content, flags=re.DOTALL)
    
    new_content = f"""{{% extends "base.html" %}}

{{% block title %}}{page_title}{{% endblock %}}
{{% block page_title %}}{page_title.upper()}{{% endblock %}}

{{% block content %}}
{main_content.strip()}
{{% endblock %}}

{{% block scripts %}}
<!-- Scripts here if needed -->
{{% endblock %}}
"""
    with open(filepath, 'w', encoding='utf-8') as f:
        f.write(new_content)

templates = {
    'dashboard.html': 'Dashboard',
    'access_logs.html': 'Access Logs',
    'settings.html': 'Settings',
    'users.html': 'Users & Roles',
    'superadmin_logs.html': 'Audit Logs'
}

for filename, title in templates.items():
    filepath = os.path.join('templates', filename)
    if os.path.exists(filepath):
        process_file(filepath, title)
        print(f"Processed {filename}")
        
print("Template refactoring complete.")
