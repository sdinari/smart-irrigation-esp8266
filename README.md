# 🌱 Smart Irrigation ESP8266

An intelligent irrigation system based on ESP8266 with web interface for programming and remote control of garden watering.

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Arduino](https://img.shields.io/badge/Arduino-Compatible-blue.svg)](https://www.arduino.cc/)
[![ESP8266](https://img.shields.io/badge/ESP8266-Compatible-green.svg)](https://www.espressif.com/en/products/socs/esp8266)

![image](https://github.com/user-attachments/assets/4db0aeb7-0176-4362-8801-bc144c1cd17d)
![image](https://github.com/user-attachments/assets/fd1bb439-24c1-4162-bde8-3599d041ab0e)
![image](https://github.com/user-attachments/assets/b432e233-3a41-48c6-907d-93fb21eea746)



## ✨ Features

- 🕒 **Flexible Programming**: Up to 8 customizable irrigation programs
- 📅 **Weekly Scheduling**: Select specific days of the week for each program
- 🌐 **Web Interface**: Configuration and control via web browser
- 📱 **Captive Portal**: Simplified WiFi setup on first boot
- ⏰ **NTP Synchronization**: Automatic time with timezone support
- 🎛️ **Manual Control**: Start/stop irrigation via web interface or serial commands
- 💾 **EEPROM Storage**: Persistent configuration even after power loss
- 🔄 **Auto-restart**: System recovery and reconnection capabilities
- 🌍 **Multi-language**: Interface available in French (easily adaptable)

## 🔧 Hardware Requirements

- **ESP8266** board (NodeMCU, Wemos D1 Mini, etc.)
- **Relay module** (to control solenoid valve)
- **Solenoid valve** for irrigation
- **12V/24V power supply** for the solenoid valve
- **5V power supply** for ESP8266
- **Jumper wires** and breadboard/PCB

## 📋 Wiring Diagram

```
ESP8266 (NodeMCU)    Relay Module
Pin D2 (GPIO4)   ->  IN
3.3V             ->  VCC
GND              ->  GND

Relay Module     Solenoid Valve
COM              ->  Power Supply +
NO               ->  Solenoid Valve +
                    (Solenoid Valve - to Power Supply -)
```

## 🚀 Installation

### 1. Arduino IDE Setup
- Arduino IDE 1.8.x or newer
- ESP8266 Board Package installed
- Select your ESP8266 board (e.g., NodeMCU 1.0)

### 2. Required Libraries
All libraries are included with the ESP8266 package:
- `ESP8266WiFi` - WiFi connectivity
- `ESP8266WebServer` - Web server functionality
- `EEPROM` - Configuration storage
- `DNSServer` - Captive portal support

### 3. Upload Process
1. Open `smart-irrigation-esp8266.ino` in Arduino IDE
2. Select your ESP8266 board and COM port
3. Upload the sketch
4. Open Serial Monitor at 115200 baud to see status

## 📖 Quick Start Guide

### First Setup
1. **Power on** your ESP8266 - it will create a WiFi hotspot named "ESP8266-Irrigation"
2. **Connect** your phone/computer to this network
3. **Configure** - A setup page will open automatically (or go to 192.168.4.1)
4. **Enter** your WiFi credentials and relay pin number
5. **Save** - The ESP8266 will restart and connect to your network

### Web Interface Access
Once connected to your WiFi network, access the web interface using the IP address shown in the Serial Monitor.

#### Main Pages:
- **Home** (`/`) - System status and manual controls
- **Programs** (`/programs`) - Create and manage irrigation schedules
- **Configuration** (`/config`) - WiFi and system settings

### Serial Commands (115200 baud)
- `reset` - Reset configuration and restart in setup mode
- `start` - Start manual irrigation for 5 minutes
- `stop` - Stop current irrigation

## ⚙️ Configuration

### Irrigation Programs
Each program can be configured with:
- **Name**: Custom identifier for the program
- **Days**: Select days of the week (Sun=D, Mon=L, Tue=M, Wed=M, Thu=J, Fri=V, Sat=S)
- **Time**: Start time in 24-hour format (HH:MM)
- **Duration**: Watering duration in minutes (1-240)
- **Status**: Enable/disable the program

### Timezone Configuration
The system uses POSIX timezone notation:
- **Morocco/Casablanca**: `GMT-2` (default)
- **Paris**: `CET-1CEST,M3.5.0,M10.5.0/3`
- **New York**: `EST5EDT,M3.2.0,M11.1.0`
- **London**: `GMT0BST,M3.5.0/1,M10.5.0`

## 🖥️ Web Interface Screenshots

### Status Page
- Real-time irrigation status
- System information (WiFi, IP, time)
- Manual control buttons
- Active programs counter

### Program Management
- Visual day selection interface
- Time and duration settings
- Enable/disable toggles
- Add/edit/delete programs

### Configuration
- WiFi credentials management
- Relay pin configuration
- System reset options

## 🛠️ Customization

### Changing Relay Pin
- **Via Web Interface**: Go to Configuration page
- **In Code**: Modify `config.relayPin = 4;` (GPIO4 = D2 on NodeMCU)

### Adding More Programs
Increase the array size in the code:
```cpp
IrrigationProgram programs[16]; // For 16 programs instead of 8
```

### Custom Time Zones
Add your timezone in POSIX format to the configuration.

## 🔍 Troubleshooting

### WiFi Connection Issues
- ✅ Ensure your network is 2.4GHz (not 5GHz)
- ✅ Check SSID and password are correct
- ✅ Use `reset` command to reconfigure
- ✅ Check router firewall settings

### Irrigation Not Starting
- ✅ Verify relay wiring connections
- ✅ Check program is enabled and scheduled correctly
- ✅ Confirm system time is correct
- ✅ Test with manual irrigation

### Web Interface Not Accessible
- ✅ Check IP address in Serial Monitor
- ✅ Ensure port 80 is not blocked
- ✅ Try connecting from same network
- ✅ Clear browser cache

### Common Error Messages
- `WiFi connection failed` - Check credentials and signal strength
- `NTP sync failed` - Check internet connection
- `EEPROM read error` - May need to reset configuration

## 📊 Technical Specifications

- **Platform**: ESP8266 (Arduino Framework)
- **Memory Usage**: ~60KB program, ~1KB EEPROM
- **Web Server**: ESP8266WebServer (Port 80)
- **Time Sync**: NTP with timezone support
- **Max Programs**: 8 (configurable)
- **Relay Control**: Digital output (configurable pin)
- **Interface Language**: French (easily adaptable)

## 🔄 Version History

### v1.0 (Current)
- ✅ Complete web interface with responsive design
- ✅ Captive portal setup mode
- ✅ Up to 8 programmable irrigation schedules
- ✅ Manual irrigation control
- ✅ EEPROM configuration storage
- ✅ NTP time synchronization
- ✅ Serial command interface
- ✅ Automatic recovery and reconnection

## 🛣️ Roadmap

### Planned Features
- 📊 Irrigation history and statistics
- 📱 Mobile app companion
- 🌡️ Soil moisture sensor integration
- 🌦️ Weather API integration for smart scheduling
- 📧 Email/SMS notifications
- 📈 Water usage analytics
- 🔗 Home Assistant integration
- 🎨 Theme customization

## 🤝 Contributing

Contributions are welcome! Please feel free to:
- 🐛 Report bugs
- 💡 Suggest new features
- 📖 Improve documentation
- 🔧 Submit pull requests

### Development Setup
1. Fork the repository
2. Create a feature branch: `git checkout -b feature/amazing-feature`
3. Commit changes: `git commit -m 'Add amazing feature'`
4. Push to branch: `git push origin feature/amazing-feature`
5. Open a Pull Request

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 🙏 Acknowledgments

- ESP8266 Community for excellent documentation
- Arduino IDE team for the development environment
- Contributors and testers who helped improve this project

## 📞 Support

- 📧 **Issues**: Use GitHub Issues for bug reports and feature requests
- 💬 **Discussions**: Use GitHub Discussions for questions and ideas
- 📚 **Documentation**: Check the `/docs` folder for detailed guides

## ⚠️ Important Notes

- This project is designed for personal and educational use
- Ensure compliance with local water usage regulations
- Electrical installations should be done by qualified personnel
- Always test the system before leaving it unattended
- The system requires a stable WiFi connection for proper operation

---

**Made with ❤️ for smart gardening**

*If this project helped you, please consider giving it a ⭐ star on GitHub!*
