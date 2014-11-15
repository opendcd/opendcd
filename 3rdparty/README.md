

```bash
  find openfst-src-03/src/ openfst-src/   -type f ! -name '*.h' -print \
  | sed -e 's|.*/||' | diff -X - -r openfst-src-03/src/ openfst-src/src/
```
