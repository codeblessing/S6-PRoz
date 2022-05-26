import os

lines = []
for path, _, files in os.walk('./logs'):
    for file in files:
        if "shared" in file:
            continue
        with open(os.path.join(path, file)) as src:
            [lines.append(line) for line in src.readlines()]

with open('./logs/shared.log', 'w+') as log:
    log.writelines(filter(lambda line: not line.isspace() and line != "", lines))