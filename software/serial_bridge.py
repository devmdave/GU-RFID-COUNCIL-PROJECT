import json
import time
import threading
import queue
from flask import Flask, request, jsonify, Response
import serial
import serial.tools.list_ports
import socket
import subprocess

app = Flask(__name__)

ser_lock = threading.Lock()
active_ser = None
active_port = None
auto_detected_port = None
stream_queues = []

def is_nodemcu(p):
    desc = (p.description or "").upper()
    hwid = (p.hwid or "").upper()
    mfg = (p.manufacturer or "").upper()
    
    if "CH340" in desc or "CP210" in desc or "NODEMCU" in desc or "ESP8266" in desc:
        return True
    if "1A86:7523" in hwid or "10C4:EA60" in hwid:
        return True
    if "WCH.CN" in mfg or "SILICON LABS" in mfg:
        return True
    return False

def scan_and_connect():
    global active_ser, active_port, auto_detected_port
    
    ports = serial.tools.list_ports.comports()
    port_devices = [p.device for p in ports]
    
    if active_port and active_port not in port_devices:
        with ser_lock:
            if active_ser:
                try:
                    active_ser.close()
                except:
                    pass
            active_ser = None
            active_port = None
            auto_detected_port = None
            
            for q in list(stream_queues):
                try:
                    q.put_nowait({"type": "status", "connected": False})
                except queue.Full:
                    pass
                    
    if not active_port:
        nodemcu_port = None
        for p in ports:
            if is_nodemcu(p):
                nodemcu_port = p.device
                break
                
        if nodemcu_port:
            try:
                print("\nDetected serial devices:")
                for p in ports:
                    print(f"\n{p.device}")
                    print(f"  description: {p.description}")
                    print(f"  manufacturer: {p.manufacturer}")
                    if p.vid:
                        print(f"  VID: {p.vid:04X}")
                        print(f"  PID: {p.pid:04X}")
                print(f"\nSelected:\n{nodemcu_port}\n")
                
                with ser_lock:
                    active_ser = serial.Serial(nodemcu_port, 115200, timeout=1)
                    active_port = nodemcu_port
                    auto_detected_port = nodemcu_port
                    
                status_msg = {
                    "type": "status",
                    "connected": True,
                    "port": nodemcu_port,
                    "auto_detected": True
                }
                for q in list(stream_queues):
                    try:
                        q.put_nowait(status_msg)
                    except queue.Full:
                        pass
            except Exception as e:
                pass

def serial_reader_thread():
    global active_ser, active_port, auto_detected_port
    buffer = ""
    while True:
        scan_and_connect()
        
        with ser_lock:
            if active_ser and active_ser.is_open:
                try:
                    if active_ser.in_waiting:
                        chunk = active_ser.read(active_ser.in_waiting).decode('utf-8', errors='replace')
                        buffer += chunk
                        while '\n' in buffer:
                            line, buffer = buffer.split('\n', 1)
                            line = line.strip()
                            if line and "PASSWORD=" not in line.upper():
                                for q in list(stream_queues):
                                    try:
                                        q.put_nowait({"type": "serial", "line": line})
                                    except queue.Full:
                                        pass
                except Exception as e:
                    try:
                        active_ser.close()
                    except:
                        pass
                    active_ser = None
                    active_port = None
                    auto_detected_port = None
                    buffer = ""
                    for q in list(stream_queues):
                        try:
                            q.put_nowait({"type": "serial", "line": f"ERROR: Serial disconnected ({str(e)})"})
                            q.put_nowait({"type": "status", "connected": False})
                        except queue.Full:
                            pass
        time.sleep(0.05)

threading.Thread(target=serial_reader_thread, daemon=True).start()

@app.route('/ports', methods=['GET'])
def get_ports():
    ports = serial.tools.list_ports.comports()
    devices = []
    for p in ports:
        is_node = is_nodemcu(p)
        devices.append({
            "port": p.device,
            "device": "NodeMCU" if is_node else "Unknown",
            "detected": is_node,
            "vid": f"{p.vid:04X}" if p.vid else "",
            "pid": f"{p.pid:04X}" if p.pid else "",
            "manufacturer": p.manufacturer or "",
            "description": p.description or ""
        })
        
    return jsonify({
        "success": True,
        "devices": devices,
        "selected_port": active_port,
        "auto_detected": bool(active_port and active_port == auto_detected_port)
    })

@app.route('/stream')
def stream():
    port = request.args.get('port')
    
    global active_ser, active_port, auto_detected_port
    
    if port and port != "auto":
        with ser_lock:
            if active_port != port:
                if active_ser:
                    try:
                        active_ser.close()
                    except:
                        pass
                try:
                    active_ser = serial.Serial(port, 115200, timeout=1)
                    active_port = port
                    auto_detected_port = None
                    
                    status_msg = {
                        "type": "status",
                        "connected": True,
                        "port": port,
                        "auto_detected": False
                    }
                    for q in list(stream_queues):
                        try:
                            q.put_nowait(status_msg)
                        except queue.Full:
                            pass
                except Exception as e:
                    return jsonify({"error": str(e)}), 400

    def generate():
        q = queue.Queue(maxsize=100)
        stream_queues.append(q)
        
        q.put_nowait({
            "type": "status",
            "connected": bool(active_port),
            "port": active_port,
            "auto_detected": bool(active_port and active_port == auto_detected_port)
        })
        
        try:
            while True:
                try:
                    msg = q.get(timeout=10)
                    if msg is None:
                        break
                    yield f"data: {json.dumps(msg)}\n\n"
                except queue.Empty:
                    yield ": ping\n\n"
        except GeneratorExit:
            pass
        finally:
            if q in stream_queues:
                stream_queues.remove(q)
                
    return Response(generate(), mimetype='text/event-stream')

@app.route('/network-status', methods=['GET'])
def network_status():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(('8.8.8.8', 80))
        ip = s.getsockname()[0]
    except Exception:
        ip = None
    finally:
        s.close()
        
    ssid = None
    interface = None
    try:
        output = subprocess.check_output('netsh wlan show interfaces', text=True)
        for line in output.split('\n'):
            line = line.strip()
            if line.startswith('Name'):
                interface = line.split(':', 1)[1].strip()
            elif line.startswith('SSID') and not line.startswith('BSSID'):
                ssid = line.split(':', 1)[1].strip()
    except Exception:
        pass
        
    status = "connected" if ip else "disconnected"
    return jsonify({
        "status": status,
        "ip": ip,
        "ssid": ssid,
        "interface": interface
    })

@app.route('/configure', methods=['POST'])
def configure():
    data = request.get_json()
    if not data:
        return jsonify({"success": False, "error": "Missing payload"}), 400
        
    port = data.get('port')
    wifi_ssid = data.get('wifi_ssid')
    wifi_password = data.get('wifi_password')
    server_url = data.get('server_url')
    
    global active_ser, active_port, auto_detected_port
    
    if port == 'auto' or not port:
        port = active_port
        
    if not port:
        return jsonify({"success": False, "error": "No NodeMCU connected or port selected"}), 400
        
    if not wifi_ssid or not wifi_password or not server_url:
        return jsonify({"success": False, "error": "All configuration fields are required"}), 400
        
    with ser_lock:
        if active_port != port:
            if active_ser:
                try:
                    active_ser.close()
                except:
                    pass
            try:
                active_ser = serial.Serial(port, 115200, timeout=2)
                active_port = port
                auto_detected_port = None
                time.sleep(2)
            except serial.SerialException as e:
                return jsonify({"success": False, "error": f"Unable to connect to NodeMCU on {port}. Is it busy?"}), 400
                
        def send_cmd(cmd):
            active_ser.write(f"{cmd}\n".encode())
            time.sleep(0.5)
            response = ""
            while active_ser.in_waiting:
                chunk = active_ser.read(active_ser.in_waiting).decode('utf-8', errors='ignore')
                response += chunk
                
                for l in chunk.split('\n'):
                    l = l.strip()
                    if l and "PASSWORD=" not in l.upper():
                        for q in list(stream_queues):
                            try:
                                q.put_nowait({"type": "serial", "line": l})
                            except queue.Full:
                                pass
                            
            return response
            
        send_cmd("CONFIG")
        send_cmd(f"SSID={wifi_ssid}")
        send_cmd(f"PASSWORD={wifi_password}")
        send_cmd(f"SERVER={server_url}")
        save_resp = send_cmd("SAVE")
        
        if "CONFIGURATION SAVED SUCCESSFULLY" in save_resp.upper() or "CONFIGURATION SAVED" in save_resp.upper():
            return jsonify({"success": True, "message": "Device configuration saved successfully"})
        elif not save_resp:
             return jsonify({"success": False, "error": "SAVE confirmation not received from NodeMCU."}), 400
        else:
            return jsonify({"success": True, "message": "Device configuration saved successfully (unconfirmed)"})

if __name__ == '__main__':
    print("Serial Bridge running on 127.0.0.1:8765")
    app.run(host='127.0.0.1', port=8765, threaded=True)
