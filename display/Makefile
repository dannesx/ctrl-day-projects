PORT ?= 8080
.PHONY: dev test
dev:
	python3 -m http.server $(PORT) --bind 127.0.0.1
test:
	node --test tests/core.test.cjs
	python3 -m unittest discover -s tests -p "test_*.py" -v
