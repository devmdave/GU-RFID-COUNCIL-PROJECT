import json
import time
from flask import Flask, request, jsonify
import serial
import serial.tools.list_ports

app = Flask(__name__)

@app.route('/ports', methods=['GET'])
def get_ports():
    ports = serial.tools.list_ports.comports()
    port_list = [{"device": p.device, "description": p.description} for p in ports]
    return jsonify({"success": True, "ports": port_list})

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
        
    try:
        ser = serial.Serial(port, 115200, timeout=2)
        time.sleep(2)
        
        def send_cmd(cmd):
            ser.write(f"{cmd}\n".encode())
            time.sleep(0.5)
            response = ""
            while ser.in_waiting:
                response += ser.read(ser.in_waiting).decode(errors='ignore')
            return response
            
        send_cmd("CONFIG")
        
        send_cmd(f"SSID={wifi_ssid}")
        send_cmd(f"PASSWORD={wifi_password}")
        send_cmd(f"SERVER={server_url}")
        
        save_resp = send_cmd("SAVE")
        ser.close()
        
        if "CONFIGURATION SAVED SUCCESSFULLY" in save_resp.upper() or "CONFIGURATION SAVED" in save_resp.upper():
            return jsonify({"success": True, "message": "Device configuration saved successfully"})
        elif not save_resp:
             return jsonify({"success": False, "error": "SAVE confirmation not received from NodeMCU."}), 400
        else:
            return jsonify({"success": True, "message": "Device configuration saved successfully (unconfirmed)"})

    except serial.SerialException as e:
        return jsonify({"success": False, "error": f"Unable to connect to NodeMCU on {port}. Is it busy?"}), 400
    except Exception as e:
        return jsonify({"success": False, "error": str(e)}), 500

if __name__ == '__main__':
    print("Serial Bridge running on 127.0.0.1:8765")
    app.run(host='127.0.0.1', port=8765)
