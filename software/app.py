import os
import sys
from flask import Flask, request, jsonify
from flask_sqlalchemy import SQLAlchemy

app = Flask(__name__)

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

@app.route('/verify', methods=['POST'])
def verify():
    data = request.get_json()
    if not data or 'number' not in data:
        return jsonify({"success": False, "error": "Missing number in payload"}), 400
    
    number = str(data['number'])
    
    card = Card.query.filter_by(number=number).first()
    
    if card and card.active:
        print(f"Received number: {number}", file=sys.stderr)
        print("Access: GRANTED", file=sys.stderr)
        return jsonify({"success": True, "access": "granted"})
    else:
        print(f"Received number: {number}", file=sys.stderr)
        print("Access: DENIED", file=sys.stderr)
        return jsonify({"success": True, "access": "denied"})

if __name__ == '__main__':
    # Run the app on all interfaces, port 5000
    app.run(host='0.0.0.0', port=5000)
