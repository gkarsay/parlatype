usage:
	@echo "Build locally:           'make build'"
	@echo "Serve at localhost:4000: 'make serve'"

build:
	bundle exec jekyll build --config=_local_config.yml

serve:
	bundle exec jekyll serve

.PHONY: usage build serve

