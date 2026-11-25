import json
import signal
import time
from http.server import BaseHTTPRequestHandler, HTTPServer

import pika

stopping = False


def stop(*_):
    global stopping
    stopping = True


def generate(tasks, count, length, use_symbols):
    params = pika.ConnectionParameters("rabbitmq", stack_timeout=3, blocked_connection_timeout=3)
    with pika.BlockingConnection(params) as connection:
        channel = connection.channel()
        channel.queue_declare(queue="password_jobs")
        reply_queue = channel.queue_declare(queue="", exclusive=True).method.queue
        body = json.dumps([count, length, use_symbols])
        properties = pika.BasicProperties(reply_to=reply_queue)

        for _ in range(tasks):
            channel.basic_publish("", "password_jobs", body, properties)

        results = []
        deadline = time.monotonic() + 5
        for method, _, body in channel.consume(reply_queue, auto_ack=True, inactivity_timeout=0.2):
            if stopping or time.monotonic() >= deadline:
                raise TimeoutError
            if method is not None:
                results.append(json.loads(body))
                if len(results) == tasks:
                    return results


class Handler(BaseHTTPRequestHandler):
    timeout = 2

    def reply(self, status, data):
        body = json.dumps(data).encode()
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_POST(self):
        if self.path != "/generate":
            self.send_error(404)
            return

        try:
            size = int(self.headers.get("Content-Length", "0"))
            if not 0 < size <= 4096:
                raise ValueError
            data = json.loads(self.rfile.read(size))
            tasks = data.get("tasks", 1)
            count = data.get("count", 5)
            length = data.get("length", 16)
            use_symbols = data.get("use_symbols", True)
            for value, low, high in ((tasks, 1, 200), (count, 1, 100), (length, 4, 128)):
                if type(value) is not int or not low <= value <= high:
                    raise ValueError
            if type(use_symbols) is not bool:
                raise ValueError
        except (AttributeError, ValueError):
            self.reply(400, {"error": "Use JSON with tasks (1–200), count (1–100), length (4–128), use_symbols (boolean)"})
            return

        try:
            results = generate(tasks, count, length, use_symbols)
        except (TimeoutError, pika.exceptions.AMQPError):
            self.reply(503, {"error": "Password workers unavailable"})
            return
        self.reply(200, {"results": results})


if __name__ == "__main__":
    signal.signal(signal.SIGTERM, stop)
    signal.signal(signal.SIGINT, stop)
    with HTTPServer(("0.0.0.0", 8080), Handler) as server:
        server.timeout = 0.2
        while not stopping:
            server.handle_request()
