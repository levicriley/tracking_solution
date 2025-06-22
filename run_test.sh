sh build.sh
docker run --rm   -v "$PWD":/app   -w /app   --entrypoint ./tests/run_tests_docker.sh   tracking-solution