import os
import sys
import datetime
from flask import Flask, request, jsonify, render_template, redirect, url_for, session
from flask_sqlalchemy import SQLAlchemy

app = Flask(__name__)
app.secret_key = 'super_secret_rfid_key' # For demo session

# Load database URI from environment variable
app.config['SQLALCHEMY_DATABASE_URI'] = os.environ.get('DATABASE_URL', 'postgresql+psycopg2://postgres:postgres@postgres:5432/rfid_db')
app.config['SQLALCHEMY_TRACK_MODIFICATIONS'] = False

db = SQLAlchemy(app)

class Card(db.Model):
    __tablename__ = 'cards'
    id = db.Column(db.Integer, primary_key=True)
    number = db.Column(db.String(50), unique=True, nullable=False)
    active = db.Column(db.Boolean, default=True, nullable=False)

with app.app_context():
    db.create_all()

# Store the latest scan in memory to be polled by the frontend
latest_scan_data = {
    "number": None,
    "access": None,
    "timestamp": None,
    "id": 0
}

@app.route('/')
def index():
    return redirect(url_for('login'))

@app.route('/login', methods=['GET', 'POST'])
def login():
    if request.method == 'POST':
        username = request.form.get('username')
        password = request.form.get('password')
        if username == 'admin' and password == 'admin123':
            session['logged_in'] = True
            return redirect(url_for('dashboard'))
        else:
            return render_template('login.html', error="Invalid credentials")
    
    if session.get('logged_in'):
        return redirect(url_for('dashboard'))
        
    return render_template('login.html')

@app.route('/logout')
def logout():
    session.pop('logged_in', None)
    return redirect(url_for('login'))

@app.route('/dashboard')
def dashboard():
    if not session.get('logged_in'):
        return redirect(url_for('login'))
    return render_template('dashboard.html')

@app.route('/verify', methods=['POST'])
def verify():
    data = request.get_json()
    if not data or 'number' not in data:
        return jsonify({"success": False, "error": "Missing number in payload"}), 400
    
    number = str(data['number'])
    
    card = Card.query.filter_by(number=number).first()
    
    access_status = "granted" if card and card.active else "denied"
    
    global latest_scan_data
    latest_scan_data["id"] += 1
    latest_scan_data["number"] = number
    latest_scan_data["access"] = access_status
    latest_scan_data["timestamp"] = datetime.datetime.now().strftime("%H:%M:%S")

    print(f"Received number: {number}", file=sys.stderr)
    print(f"Access: {access_status.upper()}", file=sys.stderr)
    
    return jsonify({"success": True, "access": access_status})

@app.route('/latest-scan', methods=['GET'])
def latest_scan():
    if not session.get('logged_in'):
        return jsonify({"error": "Unauthorized"}), 401
    return jsonify(latest_scan_data)

if __name__ == '__main__':
    # Run the app on all interfaces, port 5000
    app.run(host='0.0.0.0', port=5000)
