import os
import sys
import datetime
import json
import queue
import re
import time
import requests
from functools import wraps
from flask import Flask, request, jsonify, render_template, redirect, url_for, session, abort, Response, send_file
import io
import openpyxl
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
    name = db.Column(db.String(100), nullable=True)
    enrollment = db.Column(db.String(50), nullable=True)
    role = db.Column(db.String(50), nullable=True)
    committee = db.Column(db.String(50), nullable=True)

class AccessLog(db.Model):
    __tablename__ = 'access_logs'
    id = db.Column(db.Integer, primary_key=True)
    number = db.Column(db.String(50), nullable=False)
    name = db.Column(db.String(100), nullable=True)
    enrollment = db.Column(db.String(50), nullable=True)
    role = db.Column(db.String(50), nullable=True)
    committee = db.Column(db.String(50), nullable=True)
    access = db.Column(db.String(20), nullable=False)
    attendance = db.Column(db.String(20), nullable=True)
    timestamp = db.Column(db.DateTime, default=datetime.datetime.utcnow)

# SSE client queues
sse_clients = []

# Global memory for NodeMCU IP
last_known_nodemcu_ip = None

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

@app.route('/access-logs')
@login_required
def access_logs_page():
    return render_template('access_logs.html')

@app.route('/settings')
@login_required
@role_required('superadmin', 'admin')
def settings():
    return render_template('settings.html')

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
    global last_known_nodemcu_ip
    last_known_nodemcu_ip = request.remote_addr
    
    data = request.get_json()
    if not data or 'number' not in data:
        return jsonify({"success": False, "error": "Missing number in payload"}), 400
    
    number = str(data['number']).strip()
    if not number:
        return jsonify({"success": False, "error": "Empty number"}), 400
        
    card = Card.query.filter_by(number=number).first()
    if not card or not card.active:
        return jsonify({"success": False, "access": "denied", "message": "Card not found or inactive"})
        
    now = datetime.datetime.utcnow()
    
    if card.enrollment:
        latest_log = AccessLog.query.filter_by(enrollment=card.enrollment).order_by(AccessLog.timestamp.desc()).first()
    else:
        latest_log = AccessLog.query.filter_by(number=number).order_by(AccessLog.timestamp.desc()).first()
        
    if latest_log and latest_log.access == 'ENTRY' and latest_log.attendance == 'PENDING':
        elapsed = (now - latest_log.timestamp).total_seconds()
        if elapsed >= 15 * 60:
            access_status = "EXIT"
            attendance_status = "COUNTED"
            logged = True
            
            new_log = AccessLog(
                number=number,
                name=card.name,
                enrollment=card.enrollment,
                role=card.role,
                committee=card.committee,
                access=access_status,
                attendance=attendance_status,
                timestamp=now
            )
            db.session.add(new_log)
            db.session.commit()
            
            event_data = {
                "id": new_log.id,
                "number": number,
                "name": card.name,
                "enrollment": card.enrollment,
                "role": card.role,
                "committee": card.committee,
                "access": access_status,
                "attendance": attendance_status,
                "timestamp": new_log.timestamp.strftime("%H:%M:%S")
            }
            
            for q in sse_clients:
                q.put({"success": True, "scan": event_data})
                
            return jsonify({
                "success": True,
                "access": "exit",
                "attendance": "counted",
                "logged": True
            })
        else:
            event_data = {
                "number": number,
                "name": card.name,
                "access": "blocked",
                "message": "Minimum 15 minutes are required between Entry and Exit."
            }
            for q in sse_clients:
                q.put({"success": True, "scan": event_data, "blocked": True})
                
            return jsonify({
                "success": True,
                "access": "blocked",
                "attendance": "not_counted",
                "logged": False,
                "message": "Attendance will not be counted. Minimum 15 minutes are required between Entry and Exit."
            })
    else:
        access_status = "ENTRY"
        attendance_status = "PENDING"
        logged = True
        
        new_log = AccessLog(
            number=number,
            name=card.name,
            enrollment=card.enrollment,
            role=card.role,
            committee=card.committee,
            access=access_status,
            attendance=attendance_status,
            timestamp=now
        )
        db.session.add(new_log)
        db.session.commit()
        
        event_data = {
            "id": new_log.id,
            "number": number,
            "name": card.name,
            "enrollment": card.enrollment,
            "role": card.role,
            "committee": card.committee,
            "access": access_status,
            "attendance": attendance_status,
            "timestamp": new_log.timestamp.strftime("%H:%M:%S")
        }
        
        for q in sse_clients:
            q.put({"success": True, "scan": event_data})
            
        return jsonify({
            "success": True,
            "access": "entry",
            "attendance": "pending",
            "logged": True
        })

@app.route('/api/access-logs', methods=['GET'])
@login_required
def access_logs():
    query = AccessLog.query
    
    date_str = request.args.get('date')
    if date_str:
        try:
            target_date = datetime.datetime.strptime(date_str, '%Y-%m-%d').date()
            start_dt = datetime.datetime.combine(target_date, datetime.datetime.min.time())
            end_dt = datetime.datetime.combine(target_date, datetime.datetime.max.time())
            query = query.filter(AccessLog.timestamp >= start_dt, AccessLog.timestamp <= end_dt)
        except ValueError:
            pass
            
    query = query.order_by(AccessLog.timestamp.desc())
    
    limit_str = request.args.get('limit')
    if limit_str != 'all':
        try:
            limit = int(limit_str) if limit_str else 15
            query = query.limit(limit)
        except ValueError:
            query = query.limit(15)
            
    logs = query.all()
    logs_data = [{
        "id": log.id,
        "number": log.number,
        "name": log.name,
        "enrollment": log.enrollment,
        "role": log.role,
        "committee": log.committee,
        "access": log.access,
        "attendance": log.attendance,
        "timestamp": log.timestamp.strftime("%Y-%m-%d %H:%M:%S")
    } for log in logs]
    
    return jsonify({
        "success": True,
        "logs": logs_data
    })

@app.route('/api/access-logs/export', methods=['GET'])
@login_required
def export_access_logs():
    query = AccessLog.query
    date_str = request.args.get('date')
    filename = "access_logs_all.xlsx"
    
    if date_str:
        try:
            target_date = datetime.datetime.strptime(date_str, '%Y-%m-%d').date()
            start_dt = datetime.datetime.combine(target_date, datetime.datetime.min.time())
            end_dt = datetime.datetime.combine(target_date, datetime.datetime.max.time())
            query = query.filter(AccessLog.timestamp >= start_dt, AccessLog.timestamp <= end_dt)
            filename = f"access_logs_{date_str}.xlsx"
        except ValueError:
            pass
            
    logs = query.order_by(AccessLog.timestamp.desc()).all()
    
    wb = openpyxl.Workbook()
    ws = wb.active
    ws.title = "Access Logs"
    
    headers = ["Name", "Enrollment", "Role", "Committee", "Timestamp", "Access", "Attendance"]
    ws.append(headers)
    
    for log in logs:
        ws.append([
            log.name or '-',
            log.enrollment or '-',
            log.role or '-',
            log.committee or '-',
            log.timestamp.strftime("%Y-%m-%d %H:%M:%S") if log.timestamp else '-',
            log.access or '-',
            log.attendance or '-'
        ])
        
    output = io.BytesIO()
    wb.save(output)
    output.seek(0)
    
    return send_file(
        output,
        as_attachment=True,
        download_name=filename,
        mimetype='application/vnd.openxmlformats-officedocument.spreadsheetml.sheet'
    )

@app.route('/api/scan-events', methods=['GET'])
@login_required
def scan_events():
    def stream():
        q = queue.Queue()
        sse_clients.append(q)
        try:
            while True:
                try:
                    data = q.get(timeout=15)
                    yield f"data: {json.dumps(data)}\n\n"
                except queue.Empty:
                    yield ": ping\n\n"
        except GeneratorExit:
            pass
        finally:
            if q in sse_clients:
                sse_clients.remove(q)

    return Response(stream(), mimetype='text/event-stream')

def is_valid_url(url):
    regex = re.compile(
        r'^https?://'
        r'(?:(?:[A-Z0-9](?:[A-Z0-9-]{0,61}[A-Z0-9])?\.)+[A-Z]{2,6}\.?|'
        r'localhost|'
        r'\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3})'
        r'(?::\d+)?'
        r'(?:/?|[/?]\S+)$', re.IGNORECASE)
    return url is not None and regex.search(url)

@app.route('/api/device/serial-ports', methods=['GET'])
@login_required
def get_serial_ports():
    try:
        r = requests.get('http://host.docker.internal:8765/ports', timeout=5)
        return jsonify(r.json()), r.status_code
    except requests.RequestException:
        return jsonify({"success": False, "error": "Serial Bridge is not running on host."}), 502

@app.route('/api/device/configure-usb', methods=['POST'])
@login_required
@role_required('superadmin', 'admin')
def configure_usb():
    data = request.get_json()
    if not data:
        return jsonify({"success": False, "error": "Missing payload"}), 400
        
    try:
        r = requests.post('http://host.docker.internal:8765/configure', json=data, timeout=15)
        return jsonify(r.json()), r.status_code
    except requests.RequestException:
        return jsonify({"success": False, "error": "Serial Bridge is not running on host or timed out."}), 502

@app.route('/api/device/serial-stream', methods=['GET'])
@login_required
@role_required('superadmin', 'admin')
def serial_stream():
    port = request.args.get('port', 'auto')
    
    def generate():
        try:
            r = requests.get(f'http://host.docker.internal:8765/stream?port={port}', stream=True, timeout=60)
            if r.status_code != 200:
                try:
                    err = r.json().get("error", "Connection failed")
                except:
                    err = "Connection failed"
                yield f"data: {json.dumps({'type': 'serial', 'line': f'ERROR: {err}'})}\n\n"
                return
                
            for line in r.iter_lines(decode_unicode=True):
                if line:
                    yield f"{line}\n\n"
        except requests.RequestException:
            yield f"data: {json.dumps({'type': 'serial', 'line': 'ERROR: Serial Bridge disconnected.'})}\n\n"
            
    return Response(generate(), mimetype='text/event-stream')

@app.route('/api/device/status', methods=['GET'])
@login_required
def device_status():
    try:
        r = requests.get('http://host.docker.internal:8765/network-status', timeout=5)
        net_info = r.json()
    except Exception:
        net_info = {
            "status": "disconnected",
            "ip": None,
            "ssid": None,
            "interface": None
        }
        
    nodemcu_info = {
        "status": "offline",
        "device": "NodeMCU",
        "ip": None,
        "uptime": None
    }
    
    global last_known_nodemcu_ip
    
    if last_known_nodemcu_ip:
        nodemcu_info["ip"] = last_known_nodemcu_ip
        try:
            r = requests.get(f'http://{last_known_nodemcu_ip}/status', timeout=2)
            if r.status_code == 200:
                data = r.json()
                nodemcu_info.update(data)
                nodemcu_info["status"] = "online"
        except Exception:
            pass
            
    return jsonify({
        "success": True,
        "nodemcu": nodemcu_info,
        "network": net_info
    })

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)
