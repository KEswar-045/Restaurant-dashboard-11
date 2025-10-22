# server.py
from flask import Flask, request, jsonify, render_template
from flask_sqlalchemy import SQLAlchemy
from flask_cors import CORS
import datetime
import pandas as pd

app = Flask(__name__, template_folder="templates")
CORS(app)

app.config['SQLALCHEMY_DATABASE_URI'] = 'sqlite:///database.db'
app.config['SQLALCHEMY_TRACK_MODIFICATIONS'] = False
db = SQLAlchemy(app)

class CallLog(db.Model):
    __tablename__ = 'call_log'
    id = db.Column(db.Integer, primary_key=True)
    table_id = db.Column(db.Integer, nullable=False)
    event = db.Column(db.String(50), nullable=False)
    timestamp = db.Column(db.DateTime, default=datetime.datetime.now)

@app.route('/log', methods=['POST', 'GET'])
def log_data():
    # Accept JSON POST first
    data = request.get_json(silent=True)
    if data:
        table_id = data.get('tableId') or data.get('table')
        event = data.get('event')
        time_str = data.get('time')
    else:
        # fallback to GET query params (convenient for simple ESP GET)
        table_id = request.args.get('table') or request.args.get('tableId')
        event = request.args.get('event')
        time_str = request.args.get('time')

    if not table_id or not event:
        return jsonify({"status": "error", "message": "Missing table or event"}), 400

    try:
        table_id = int(table_id)
    except:
        return jsonify({"status":"error", "message":"table must be int"}), 400

    # parse time if given (ISO format)
    ts = None
    if time_str:
        try:
            ts = datetime.datetime.fromisoformat(time_str)
        except Exception:
            try:
                # if client sent millis timestamp
                ts = datetime.datetime.utcfromtimestamp(float(time_str)/1000.0)
            except:
                ts = None

    entry = CallLog(table_id=table_id, event=event)
    if ts:
        entry.timestamp = ts

    db.session.add(entry)
    db.session.commit()

    # also append to events.csv
    with open("events.csv", "a") as f:
        f.write(f"{entry.table_id},{entry.event},{entry.timestamp.isoformat()}\n")

    print(f"Data logged: Table {entry.table_id}, Event: {entry.event}, Time: {entry.timestamp}")
    return jsonify({"status": "success", "message": "Data logged"}), 200

@app.route('/data')
def get_all_data():
    try:
        logs_df = pd.read_sql_table('call_log', db.engine)
    except Exception:
        logs_df = pd.read_sql('SELECT * FROM call_log', db.engine)
    # basic processing: return records and a small live_status summary
    if logs_df.empty:
        return jsonify({'records': [], 'live_status': []})
    logs_df['timestamp'] = pd.to_datetime(logs_df['timestamp'])
    latest = logs_df.sort_values('timestamp').drop_duplicates(subset='table_id', keep='last')
    live_status = []
    now = datetime.datetime.now()
    for _, r in latest.iterrows():
        minutes_ago = int((now - r['timestamp'].to_pydatetime()).total_seconds() / 60)
        live_status.append({
            'table_id': int(r['table_id']),
            'event': r['event'],
            'minutes_ago': minutes_ago
        })
    return jsonify({'records': logs_df.to_dict('records'), 'live_status': live_status})

@app.route('/')
def index():
    return render_template('dashboard.html')

if __name__ == '__main__':
    with app.app_context():
        db.create_all()
    app.run(host='0.0.0.0', port=5000, debug=True)
