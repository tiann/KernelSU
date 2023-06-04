import os
import sys
import asyncio
import telegram
from telegram import helpers


BOT_TOKEN = os.environ.get("BOT_TOKEN")
CHAT_ID = os.environ.get("CHAT_ID")
CACHE_CHAT_ID = os.environ.get("CACHE_CHAT_ID")
MESSAGE_THREAD_ID = os.environ.get("MESSAGE_THREAD_ID")
COMMIT_URL = os.environ.get("COMMIT_URL")
COMMIT_MESSAGE = os.environ.get("COMMIT_MESSAGE")
RUN_URL = os.environ.get("RUN_URL")
TITLE = os.environ.get("TITLE")
VERSION = os.environ.get("VERSION")
MSG_TEMPLATE = """
*{title}*
\#ci\_{version}
```
{commit_message}
```
[Commit]({commit_url})
[Workflow run]({run_url})
""".strip()


def get_caption():
    msg = MSG_TEMPLATE.format(
        title=helpers.escape_markdown(TITLE, 2),
        version=helpers.escape_markdown(VERSION, 2),
        commit_message=helpers.escape_markdown(COMMIT_MESSAGE, 2, telegram.MessageEntity.PRE),
        commit_url=helpers.escape_markdown(COMMIT_URL, 2, telegram.MessageEntity.TEXT_LINK),
        run_url=helpers.escape_markdown(RUN_URL, 2, telegram.MessageEntity.TEXT_LINK)
    )
    if len(msg) > telegram.constants.MessageLimit.CAPTION_LENGTH:
        return COMMIT_URL
    return msg


def check_environ():
    if BOT_TOKEN is None:
        print("[-] Invalid BOT_TOKEN")
        exit(1)
    if CHAT_ID is None:
        print("[-] Invalid CHAT_ID")
        exit(1)
    if CACHE_CHAT_ID is None:
        print("[-] Invalid CACHE_CHAT_ID")
        exit(1)
    if COMMIT_URL is None:
        print("[-] Invalid COMMIT_URL")
        exit(1)
    if COMMIT_MESSAGE is None:
        print("[-] Invalid COMMIT_MESSAGE")
        exit(1)
    if RUN_URL is None:
        print("[-] Invalid RUN_URL")
        exit(1)
    if TITLE is None:
        print("[-] Invalid TITLE")
        exit(1)
    if VERSION is None:
        print("[-] Invalid VERSION")
        exit(1)


async def main():
    print("[+] Uploading to telegram")
    check_environ()
    print("[+] Files:", sys.argv[1:])
    bot = telegram.Bot(BOT_TOKEN)
    files = []
    paths = sys.argv[1:]
    caption = get_caption()
    print("[+] Caption: ")
    print("---")
    print(caption)
    print("---")
    for one in paths:
        if not os.path.exists(one):
            print("[-] File not exist: " + one)
            continue
        print("[+] Upload: " + one)
        msg = await bot.send_document(CACHE_CHAT_ID, one, write_timeout=60, connect_timeout=30)
        if one == paths[-1]:
            files.append(telegram.InputMediaDocument(msg.document,
                                                     caption=caption,
                                                     parse_mode=telegram.constants.ParseMode.MARKDOWN_V2))
        else:
            files.append(telegram.InputMediaDocument(msg.document))
        await bot.delete_message(CACHE_CHAT_ID, msg.message_id)
    print("[+] Sending")
    await bot.send_media_group(CHAT_ID, files, message_thread_id=MESSAGE_THREAD_ID)
    print("[+] Done!")


if __name__ == "__main__":
    loops = asyncio.new_event_loop()
    loops.run_until_complete(asyncio.wait([main()]))

