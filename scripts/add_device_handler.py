import json
import sys
import os


def main():
    assert len(sys.argv) == 2
    file_name = sys.argv[1]
    github = "https://github.com/"
    issue_content = os.environ["ISSUE_CONTENT"]
    lines = issue_content.split("\n\n")
    assert len(lines) == 6
    url = lines[1]
    print(url)
    device = lines[3]
    print(device)
    code_of_conduct = lines[5]
    print(code_of_conduct)
    assert code_of_conduct.find("[X]") > 0
    tmp = url.removesuffix("/").replace(github, "").split("/")
    print(tmp)
    assert len(tmp) == 2
    maintainer = tmp[0]
    print(maintainer)
    maintainer_link = "%s%s" % (github, maintainer)
    print(maintainer_link)
    kernel_name = tmp[1]
    print(kernel_name)
    kernel_link = "%s%s/%s" % (github, maintainer, kernel_name)
    print(kernel_link)
    with open(file_name, "r") as f:
        data = json.loads(f.read())
        data.append(
            {
                "maintainer": maintainer,
                "maintainer_link": maintainer_link,
                "kernel_name": kernel_name,
                "kernel_link": kernel_link,
                "devices": device,
            }
        )
    os.remove(file_name)
    with open(file_name, "w") as f:
        f.write(json.dumps(data, indent=4))
    os.system("echo success=true >> $GITHUB_OUTPUT")
    os.system("echo device=%s >> $GITHUB_OUTPUT" % device)


if __name__ == "__main__":
    main()