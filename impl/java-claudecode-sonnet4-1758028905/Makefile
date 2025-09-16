IMPL_NAME := $(notdir $(CURDIR))
IMAGE_PREFIX := local/santa-$(IMPL_NAME)
IMAGE_BUILD := $(IMAGE_PREFIX):build
IMAGE_CLI := $(IMAGE_PREFIX):cli
REPO_ROOT := $(abspath $(CURDIR)/../..)

# Optional flags for docker runs, e.g. --platform linux/amd64
DOCKER_FLAGS := $(SANTA_DOCKER_FLAGS)

.PHONY: *

help:
	@echo "Targets:"
	@echo "  build-image        - Build builder image (Dockerfile.build)"
	@echo "  cli-image          - Build CLI image (Dockerfile.cli)"
	@echo "  shell              - Interactive dev shell in builder image"
	@echo "  exec CMD=...       - Run single command in builder image"
	@echo "  run ARGS=...       - Run CLI image with args (tokens/ast/<file>)"
	@echo "  print-uri          - Print docker:// URI for santa-test"
	@echo "  test               - Run santa-test over all tests via docker"
	@echo "  test-stage-1       - Run santa-test for tests/stage-1"
	@echo "  test-stage-2       - Run santa-test for tests/stage-2"
	@echo "  test-stage-3       - Run santa-test for tests/stage-3"
	@echo "  test-stage-4       - Run santa-test for tests/stage-4"
	@echo "  test-stage-5       - Run santa-test for tests/stage-5"
	@echo "  test-file FILE=... - Run santa-test for a single .santat file"
	@echo "  clean-images       - Remove local images for this impl"

build-image:
	docker build -f Dockerfile.build -t $(IMAGE_BUILD) .

cli-image: build-image
	docker build -f Dockerfile.cli --build-arg BUILDER_IMAGE=$(IMAGE_BUILD) -t $(IMAGE_CLI) .

shell: build-image
	docker run --rm -it $(DOCKER_FLAGS) \
		-u "$$(id -u):$$(id -g)" \
		-v "$(REPO_ROOT):$(REPO_ROOT)" -w "$(REPO_ROOT)" \
		$(IMAGE_BUILD) bash

# Usage: make exec CMD="go test ./..." (always in builder)
exec: build-image
	docker run --rm $(DOCKER_FLAGS) \
		-u "$$(id -u):$$(id -g)" \
		-v "$(REPO_ROOT):$(REPO_ROOT)" -w "$(REPO_ROOT)" \
		$(IMAGE_BUILD) sh -lc '$(CMD)'

# Usage: make run ARGS="--help" or ARGS="tokens tests/stage-1/01_basic_tokens.santat"
run: cli-image
	docker run --rm $(DOCKER_FLAGS) \
		-v "$(REPO_ROOT):$(REPO_ROOT)" -w "$(REPO_ROOT)" \
		$(IMAGE_CLI) $(ARGS)

print-uri: cli-image
	@echo docker://$(IMAGE_CLI)

test: cli-image
	cd "$(REPO_ROOT)" && tools/bin/santa-test --bin docker://$(IMAGE_CLI) tests

test-stage-1: cli-image
	cd "$(REPO_ROOT)" && tools/bin/santa-test --bin docker://$(IMAGE_CLI) tests/stage-1

test-stage-2: cli-image
	cd "$(REPO_ROOT)" && tools/bin/santa-test --bin docker://$(IMAGE_CLI) tests/stage-2

test-stage-3: cli-image
	cd "$(REPO_ROOT)" && tools/bin/santa-test --bin docker://$(IMAGE_CLI) tests/stage-3

test-stage-4: cli-image
	cd "$(REPO_ROOT)" && tools/bin/santa-test --bin docker://$(IMAGE_CLI) tests/stage-4

test-stage-5: cli-image
	cd "$(REPO_ROOT)" && tools/bin/santa-test --bin docker://$(IMAGE_CLI) tests/stage-5

# Usage: make test-file FILE=tests/stage-1/01_basic_tokens.santat
test-file: cli-image
	cd "$(REPO_ROOT)" && tools/bin/santa-test --bin docker://$(IMAGE_CLI) $(FILE)

clean-images:
	- docker rmi $(IMAGE_CLI) $(IMAGE_BUILD)
