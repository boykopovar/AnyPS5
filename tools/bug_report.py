import re


IPV4 = re.compile(r"\b(?:\d{1,3}\.){3}\d{1,3}\b")


def redact(text, *, home, user, host):
    if home and len(home) >= 3:
        text = text.replace(home, "~")
    if user and len(user) >= 2:
        text = re.sub(r"(?<![\w-])" + re.escape(user) + r"(?![\w-])", "<user>", text)
    if host and len(host) >= 2:
        text = re.sub(r"(?<![\w-])" + re.escape(host) + r"(?![\w-])", "<host>", text)
    return IPV4.sub("<ip>", text)
