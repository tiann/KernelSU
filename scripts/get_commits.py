import sys
import requests


def count_commits(user, repo, branch):
    url = "https://api.github.com/repos/{}/{}/commits?sha={}&per_page=1&page=1".format(user, repo, branch)
    resp = requests.get(url)
    result = resp.headers["link"]
    result = result[result.rfind("page=") + 5: result.rfind('>; rel="last"')]
    print(result)


if __name__ == "__main__":
    if len(sys.argv) != 4:
        count_commits("tiann", "KernelSU", "main")
    else:
        count_commits(sys.argv[1], sys.argv[2], sys.argv[3])
