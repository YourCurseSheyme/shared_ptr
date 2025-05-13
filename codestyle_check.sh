#! /bin/bash

# ---| Static codestyle check and static analysis |---
echo "Running clang-format..."
find ./main.cpp | xargs clang-format --dry-run --Werror
if [ $? -ne 0 ]; then
	echo "clang-format: не пройден"
	exit 1
fi
echo "clang-format achieved"

echo "Running clang-tidy"
clang-tidy --config-file=.clang-tidy --format-style=file  -extra-arg=-fno-color-diagnostics -quiet main.cpp
if [[ ! $? -eq 0 ]]
  then 
    echo "clang-tidy: не пройден"
	exit 1
fi
echo "clang-tidy achieved"

echo "Running cpplint..."
cpplint --filter=-whitespace,-legal main.cpp
if [ $? -ne 0 ]; then
    echo "cpplint: не пройден"
    exit 1
fi
echo "cpplint achieved"

echo "Running cppcheck..."
cppcheck --enable=all --suppress=missingIncludeSystem main.cpp
if [ $? -ne 0 ]; then
    echo "cppcheck: Потенциальные ошибки."
    exit 1
fi
echo "cppcheck achieved"

echo "Running IWYU"
iwyu -Xiwyu --no_fwd_decls main.cpp
if [ $? -ne 0 ]; then
	echo "IWYU: не пройден"
	exit 1
fi
echo "IWYU achieved"

echo "Running g++ with -Wall -Werror -Wextra -pedantic check"
g++ -Wall -Werror -Wextra -pedantic -std=c++20 main.cpp -o main.out
if [[ ! -s main.out ]]
then
	echo "g++ flags не пройдены"
	exit 1
fi
echo "g++ flags achieved"
rm main.out

echo "Running clang++ with -Wall -Werror -fsanitize=address,undefined check"
clang++ -std=c++20 -Werror  -pedantic -Wall -Wextra -fsanitize=address,undefined main.cpp -o main.out
if [[ ! -s main.out ]]
then
	echo "clang flags не пройдены"
	exit 1
fi
echo "clang flags achieved"
rm main.out
