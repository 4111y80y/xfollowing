import time
import hmac
import hashlib
import base64
import urllib.parse
import requests

DING_WEBHOOK = "https://oapi.dingtalk.com/robot/send?access_token=0c41cc100f36dd995684453debf4e8690f34efa457692da609a4d663c866d67d"
DING_SECRET = "SECae54fda1927685938b81cd56c6949e3fa02b1180e4cfabc99827c79c106d6485"

def build_signed_url(webhook, secret):
    if not secret:
        return webhook
    timestamp = str(int(time.time() * 1000))
    string_to_sign = f"{timestamp}\n{secret}".encode("utf-8")
    secret_enc = secret.encode("utf-8")
    hmac_code = hmac.new(secret_enc, string_to_sign, digestmod=hashlib.sha256).digest()
    sign = urllib.parse.quote_plus(base64.b64encode(hmac_code))
    sep = "&" if "?" in webhook else "?"
    return f"{webhook}{sep}timestamp={timestamp}&sign={sign}"

def send_text(content):
    url = build_signed_url(DING_WEBHOOK, DING_SECRET)
    payload = {
        "msgtype": "text",
        "text": {"content": content}
    }
    r = requests.post(url, json=payload, timeout=10)
    r.raise_for_status()
    return r.json()

if __name__ == "__main__":
    result = send_text("[X互关宝] 测试消息 - 钉钉通知集成测试成功！")
    print("Result:", result)
