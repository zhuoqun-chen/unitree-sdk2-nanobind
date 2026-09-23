.PHONY: wheels test lint format clean

# Build both variant wheels (real and +stub) into dist/.
wheels:
	./scripts/build_wheels.sh

# Build the stub wheel and run the suite against it: uv-native, no robot or DDS.
test:
	rm -rf dist
	uv build --wheel -o dist -C cmake.define.USE_STUB_SDK=ON
	uv run --no-project --with dist/*.whl --with pytest --with numpy pytest tests/ -v

lint:
	uv run --no-project --with ruff ruff check src tests examples

format:
	uv run --no-project --with ruff ruff format src tests examples

clean:
	rm -rf build dist wheelhouse *.egg-info src/*.egg-info
