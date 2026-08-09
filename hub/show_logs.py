import urllib.request, json
d = json.load(urllib.request.urlopen('http://192.168.1.14/api/logs', timeout=5))
for e in d['logs']:
    print(f"t={e['t']} {e['msg']}")
