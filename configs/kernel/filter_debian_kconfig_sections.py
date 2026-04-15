#!/usr/bin/env python3

import argparse


def read_prefixes(prefix_file):
    prefixes = []
    with open(prefix_file, "r") as f:
        for line in f:
            line = line.strip()
            if line and not line.startswith("#"):
                prefixes.append(line)
    return prefixes


def should_skip(section_file, prefixes):
    return any(section_file.startswith(p) for p in prefixes)


def process_configs(prefixes, input_files, output_file):
    with open(output_file, "w") as out:
        for infile in input_files:
            with open(infile, "r") as f:
                current_section = []
                current_file = None

                for line in f:
                    stripped = line.strip()

                    # Detect start of a section
                    if stripped.startswith("## file:"):
                        # Flush previous section
                        if current_section and current_file is not None:
                            if not should_skip(current_file, prefixes):
                                out.writelines(current_section)

                        # Start new section
                        current_section = [line]
                        current_file = stripped[len("## file:"):].strip()

                    else:
                        current_section.append(line)

                # Flush last section
                if current_section and current_file is not None:
                    if not should_skip(current_file, prefixes):
                        out.writelines(current_section)


def main():
    parser = argparse.ArgumentParser(
        description="Filter kernel config sections based on file path prefixes"
    )
    parser.add_argument(
        "-p", "--prefix-file", required=True,
        help="File containing prefixes (one per line)"
    )
    parser.add_argument(
        "-o", "--output", required=True,
        help="Output filtered config file"
    )
    parser.add_argument(
        "inputs", nargs="+",
        help="Input kernel config file(s)"
    )

    args = parser.parse_args()

    prefixes = read_prefixes(args.prefix_file)
    process_configs(prefixes, args.inputs, args.output)


if __name__ == "__main__":
    main()
