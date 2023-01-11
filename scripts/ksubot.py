import os
import sys
import asyncio
import telegram


BOT_TOKEN = os.environ.get("BOT_TOKEN")
CHAT_ID = os.environ.get("CHAT_ID")
CACHE_CHAT_ID = os.environ.get("CACHE_CHAT_ID")
MESSAGE_THREAD_ID = os.environ.get("MESSAGE_THREAD_ID")
COMMIT_URL = os.environ.get("COMMIT_URL")
COMMIT_MESSAGE = os.environ.get("COMMIT_MESSAGE")


def get_caption():
    msg = COMMIT_MESSAGE + "\n" + COMMIT_URL
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
        msg = await bot.send_document(CACHE_CHAT_ID, one)
        if one == paths[-1]:
            files.append(telegram.InputMediaDocument(msg.document, caption=caption))
        else:
            files.append(telegram.InputMediaDocument(msg.document))
        await bot.delete_message(CACHE_CHAT_ID, msg.message_id)
    print("[+] Sending")
    await bot.send_media_group(CHAT_ID, files, message_thread_id=MESSAGE_THREAD_ID)
    print("[+] Done!")


if __name__ == "__main__":
    loops = asyncio.new_event_loop()
    loops.run_until_complete(asyncio.wait([main()]))

