import json
import secrets
import signal
import socket
import string

import pika

stopping = False
hostname = socket.gethostname()


def stop(*_):
    global stopping
    stopping = True


def password(length, use_symbols):
    groups = [string.ascii_lowercase, string.ascii_uppercase, string.digits]
    if use_symbols:
        groups.append("!@#$%^&*()-_=+")
    alphabet = "".join(groups)
    chars = [secrets.choice(group) for group in groups]
    chars.extend(secrets.choice(alphabet) for _ in range(length - len(chars)))
    secrets.SystemRandom().shuffle(chars)
    return "".join(chars)


def run():
    params = pika.ConnectionParameters("rabbitmq", stack_timeout=3, blocked_connection_timeout=3)
    with pika.BlockingConnection(params) as connection:
        channel = connection.channel()
        channel.queue_declare(queue="password_jobs")
        channel.basic_qos(prefetch_count=1)
        channel.confirm_delivery()
        print(f"{hostname} ready", flush=True)
        for method, properties, body in channel.consume("password_jobs", inactivity_timeout=0.2):
            if stopping:
                break
            if method is None:
                continue

            count, length, use_symbols = json.loads(body)
            result = {
                "worker": hostname,
                "passwords": [password(length, use_symbols) for _ in range(count)],
            }
            channel.basic_publish("", properties.reply_to, json.dumps(result))
            channel.basic_ack(method.delivery_tag)
            print(f"{hostname} generated {count} passwords", flush=True)


if __name__ == "__main__":
    signal.signal(signal.SIGTERM, stop)
    signal.signal(signal.SIGINT, stop)
    run()
    print(f"{hostname} stopped", flush=True)
