import json
import time
import threading
import queue
from flask import Flask, request, jsonify, Response
import serial
import serial.tools.list_ports

app = Flask(__name__)

ser_lock = threading.Lock()
active_ser = None
active_port = None
stream_queues = []

def serial_reader_thread():
    global active_ser, active_port
    buffer = ""
    while True:
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
                                        q.put_nowait(line)
                                    except queue.Full:
                                        pass
                except Exception as e:
                    try:
                        active_ser.close()
                    except:
                        pass
                    active_ser = None
                    active_port = None
                    buffer = ""
                    for q in list(stream_queues):
                        try:
                            q.put_nowait(f"ERROR: Serial disconnected ({str(e)})")
                            q.put_nowait(None)
                        except queue.Full:
                            pass
        time.sleep(0.05)

threading.Thread(target=serial_reader_thread, daemon=True).start()

@app.route('/ports', methods=['GET'])
def get_ports():
    ports = serial.tools.list_ports.comports()
    port_list = [{"device": p.device, "description": p.description} for p in ports]
    return jsonify({"success": True, "ports": port_list})

@app.route('/stream')
def stream():
    port = request.args.get('port')
    if not port:
        return jsonify({"error": "Missing port parameter"}), 400
        
    global active_ser, active_port
    
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
            except Exception as e:
                return jsonify({"error": str(e)}), 400

    def generate():
        q = queue.Queue(maxsize=100)
        stream_queues.append(q)
        try:
            while True:
                try:
                    line = q.get(timeout=10)
                    if line is None:
                        break
                    yield f"data: {json.dumps({'line': line})}\n\n"
                except queue.Empty:
                    yield ": ping\n\n"
        except GeneratorExit:
            pass
        finally:
            if q in stream_queues:
                stream_queues.remove(q)
                
    return Response(generate(), mimetype='text/event-stream')

@app.route('/configure', methods=['POST'])
def configure():
    data = request.get_json()
    if not data:
        return jsonify({"success": False, "error": "Missing payload"}), 400
        
    port = data.get('port')
    wifi_ssid = data.get('wifi_ssid')
    wifi_password = data.get('wifi_password')
    server_url = data.get('server_url')
    
    if not port or not wifi_ssid or not wifi_password or not server_url:
        return jsonify({"success": False, "error": "All fields are required"}), 400
        
    global active_ser, active_port
    
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
                
                # Mirror output to UI live stream while locked!
                for l in chunk.split('\n'):
                    l = l.strip()
                    if l and "PASSWORD=" not in l.upper():
                        for q in list(stream_queues):
                            try:
                                q.put_nowait(l)
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
