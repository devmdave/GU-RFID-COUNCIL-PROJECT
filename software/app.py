import os
import sys
import datetime
from functools import wraps
from flask import Flask, request, jsonify, render_template, redirect, url_for, session, abort
from flask_sqlalchemy import SQLAlchemy
from flask_migrate import Migrate
from werkzeug.security import generate_password_hash, check_password_hash

app = Flask(__name__)
app.secret_key = os.environ.get('SECRET_KEY', 'super_secret_rfid_key')

app.config['SQLALCHEMY_DATABASE_URI'] = os.environ.get('DATABASE_URL', 'postgresql+psycopg2://postgres:postgres@postgres:5432/rfid_db')
app.config['SQLALCHEMY_TRACK_MODIFICATIONS'] = False

db = SQLAlchemy(app)
migrate = Migrate(app, db)

# --- MODELS ---

class Role(db.Model):
    __tablename__ = 'roles'
    id = db.Column(db.Integer, primary_key=True)
    name = db.Column(db.String(50), unique=True, nullable=False)

class User(db.Model):
    __tablename__ = 'users'
    id = db.Column(db.Integer, primary_key=True)
    username = db.Column(db.String(100), unique=True, nullable=False)
    password_hash = db.Column(db.String(255), nullable=False)
    role_id = db.Column(db.Integer, db.ForeignKey('roles.id'), nullable=False)
    is_active = db.Column(db.Boolean, default=True, nullable=False)
    created_at = db.Column(db.DateTime, default=datetime.datetime.utcnow)
    updated_at = db.Column(db.DateTime, default=datetime.datetime.utcnow, onupdate=datetime.datetime.utcnow)
    
    role = db.relationship('Role', backref=db.backref('users', lazy=True))

class Card(db.Model):
    __tablename__ = 'cards'
    id = db.Column(db.Integer, primary_key=True)
    number = db.Column(db.String(50), unique=True, nullable=False)
    active = db.Column(db.Boolean, default=True, nullable=False)

# In-memory store for the dashboard (latest scan)
latest_scan_data = {
    "number": None,
    "access": None,
    "timestamp": None,
    "id": 0
}

# --- AUTH & RBAC DECORATORS ---

def get_current_user():
    if 'user_id' in session:
        return User.query.get(session['user_id'])
    return None

def login_required(f):
    @wraps(f)
    def decorated_function(*args, **kwargs):
        if not get_current_user():
            return redirect(url_for('login'))
        return f(*args, **kwargs)
    return decorated_function

def role_required(*allowed_roles):
    def decorator(f):
        @wraps(f)
        def decorated_function(*args, **kwargs):
            user = get_current_user()
            if not user or user.role.name not in allowed_roles:
                if request.is_json or request.path.startswith('/api'):
                    return jsonify({
                        "success": False,
                        "error": "FORBIDDEN",
                        "message": "You do not have permission to perform this action"
                    }), 403
                abort(403)
            return f(*args, **kwargs)
        return decorated_function
    return decorator

# --- ROUTES ---

@app.context_processor
def inject_user():
    return dict(current_user=get_current_user())

@app.route('/')
def index():
    return redirect(url_for('login'))

@app.route('/login', methods=['GET', 'POST'])
def login():
    if request.method == 'POST':
        username = request.form.get('username')
        password = request.form.get('password')
        
        user = User.query.filter_by(username=username).first()
        if user and user.is_active and check_password_hash(user.password_hash, password):
            session['user_id'] = user.id
            return redirect(url_for('dashboard'))
        else:
            return render_template('login.html', error="Invalid credentials or inactive account")
    
    if get_current_user():
        return redirect(url_for('dashboard'))
        
    return render_template('login.html')

@app.route('/logout')
def logout():
    session.pop('user_id', None)
    return redirect(url_for('login'))

@app.route('/dashboard')
@login_required
def dashboard():
    return render_template('dashboard.html')

@app.route('/users')
@login_required
@role_required('superadmin', 'admin')
def users_page():
    users = User.query.all()
    roles = Role.query.all()
    return render_template('users.html', users=users, roles=roles)

@app.route('/api/users', methods=['POST'])
@login_required
@role_required('superadmin', 'admin')
def create_user():
    data = request.get_json()
    username = data.get('username')
    password = data.get('password')
    role_id = data.get('role_id')
    is_active = data.get('is_active', True)
    
    role = Role.query.get(role_id)
    if not role:
        return jsonify({"success": False, "error": "Invalid role"}), 400
        
    current_user = get_current_user()
    if current_user.role.name == 'admin' and role.name == 'superadmin':
        return jsonify({"success": False, "error": "FORBIDDEN", "message": "Admins cannot create superadmins"}), 403
        
    if User.query.filter_by(username=username).first():
        return jsonify({"success": False, "error": "Username exists"}), 400
        
    new_user = User(
        username=username,
        password_hash=generate_password_hash(password),
        role_id=role.id,
        is_active=is_active
    )
    db.session.add(new_user)
    db.session.commit()
    return jsonify({"success": True})

# --- NODE MCU API ---
# Unchanged API contract

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
@login_required
def latest_scan():
    return jsonify(latest_scan_data)

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)
