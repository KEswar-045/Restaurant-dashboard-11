from flask import Flask, request, jsonify

app = Flask(__name__)

@app.route("/", methods=["GET", "POST"])
def index():
    table = request.args.get("table")
    event = request.args.get("event")
    time = request.args.get("time")
    print(f"Received -> table={table}, event={event}, time={time}")
    with open("events.csv", "a") as f:
        f.write(f"{table},{event},{time}\n")
    return jsonify(status="ok"), 200

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000, debug=True)
