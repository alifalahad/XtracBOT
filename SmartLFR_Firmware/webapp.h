#ifndef WEBAPP_H
#define WEBAPP_H

#include <Arduino.h>

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <title>Smart LFR Controller</title>
    <link href="https://fonts.googleapis.com/css2?family=Outfit:wght@300;500;700&display=swap" rel="stylesheet">
    
    <style>
        :root {
            --bg-color: #0f172a;
            --surface-color: #1e293b;
            --cell-bg: #334155;
            --accent-cyan: #06b6d4;
            --accent-glow: rgba(6, 182, 212, 0.5);
            --text-main: #f8fafc;
            --text-muted: #94a3b8;
        }

        * {
            box-sizing: border-box;
            margin: 0;
            padding: 0;
            font-family: 'Outfit', sans-serif;
            user-select: none;
            -webkit-tap-highlight-color: transparent;
        }

        body {
            background-color: var(--bg-color);
            color: var(--text-main);
            display: flex;
            flex-direction: column;
            align-items: center;
            min-height: 100vh;
            padding: 20px;
        }

        header {
            text-align: center;
            margin-bottom: 30px;
            width: 100%;
            max-width: 500px;
        }

        h1 {
            font-size: 2rem;
            font-weight: 700;
            background: linear-gradient(to right, #22d3ee, #818cf8);
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
            margin-bottom: 5px;
        }

        p.subtitle {
            color: var(--text-muted);
            font-size: 0.9rem;
        }

        .status-bar {
            background-color: var(--surface-color);
            padding: 10px 20px;
            border-radius: 20px;
            display: inline-flex;
            align-items: center;
            gap: 10px;
            margin-top: 15px;
            font-size: 0.85rem;
            font-weight: 500;
            box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.1);
        }

        .status-dot {
            width: 10px;
            height: 10px;
            border-radius: 50%;
            background-color: #fbbf24;
            box-shadow: 0 0 10px #fbbf24;
        }
        
        .status-dot.connected {
            background-color: #34d399;
            box-shadow: 0 0 10px #34d399;
        }

        .grid-container {
            display: grid;
            gap: 6px;
            background-color: var(--surface-color);
            padding: 10px;
            border-radius: 16px;
            box-shadow: 0 10px 25px rgba(0,0,0,0.5);
            margin-bottom: 30px;
        }

        .cell {
            width: 50px;
            height: 50px;
            background-color: var(--cell-bg);
            border-radius: 10px;
            cursor: pointer;
            transition: all 0.2s cubic-bezier(0.4, 0, 0.2, 1);
            position: relative;
            display: flex;
            justify-content: center;
            align-items: center;
        }

        @media (max-width: 400px) {
            .cell {
                width: 40px;
                height: 40px;
            }
        }

        .cell:active {
            transform: scale(0.9);
        }

        .cell.active {
            background-color: var(--accent-cyan);
            box-shadow: 0 0 15px var(--accent-glow);
            transform: scale(1.05);
            z-index: 2;
        }

        .cell.start-node {
            background-color: #34d399;
            box-shadow: 0 0 15px rgba(52, 211, 153, 0.5);
        }

        .cell.start-node::after {
            content: "S";
            color: #064e3b;
            font-weight: 900;
            font-size: 1.2rem;
        }

        .cell.end-node::after {
            content: "E";
            color: #164e63;
            font-weight: 900;
            font-size: 1.2rem;
        }

        .dashboard {
            width: 100%;
            max-width: 400px;
            display: flex;
            flex-direction: column;
            gap: 15px;
        }

        .command-output {
            background-color: var(--surface-color);
            padding: 15px;
            border-radius: 12px;
            font-family: monospace;
            color: var(--accent-cyan);
            min-height: 50px;
            display: flex;
            align-items: center;
            justify-content: center;
            word-break: break-all;
            text-align: center;
            font-size: 1.1rem;
            border: 1px solid rgba(6, 182, 212, 0.2);
        }

        .controls {
            display: flex;
            gap: 15px;
        }

        button {
            flex: 1;
            padding: 16px;
            border: none;
            border-radius: 12px;
            font-size: 1rem;
            font-weight: 700;
            cursor: pointer;
            transition: all 0.2s ease;
            text-transform: uppercase;
            letter-spacing: 1px;
        }

        .btn-clear {
            background-color: transparent;
            border: 2px solid #ef4444;
            color: #ef4444;
        }

        .btn-clear:hover, .btn-clear:active {
            background-color: #ef4444;
            color: white;
        }

        .btn-send {
            background: linear-gradient(135deg, #0ea5e9, #6366f1);
            color: white;
            box-shadow: 0 4px 15px rgba(99, 102, 241, 0.4);
        }

        .btn-send:hover, .btn-send:active {
            transform: translateY(-2px);
            box-shadow: 0 6px 20px rgba(99, 102, 241, 0.6);
        }

        .btn-send:disabled {
            background: var(--cell-bg);
            color: var(--text-muted);
            box-shadow: none;
            cursor: not-allowed;
            transform: none;
        }
    </style>
</head>
<body>

    <header>
        <h1>Smart Navigator</h1>
        <p class="subtitle">Draw the maze path for the robot</p>
        <div class="status-bar" id="statusBar">
            <div class="status-dot" id="statusDot"></div>
            <span id="statusText">Ready to draw</span>
        </div>
    </header>

    <div class="grid-container" id="grid">
        <!-- Grid cells injected by JS -->
    </div>

    <div class="dashboard">
        <div class="command-output" id="commandOutput">
            [WAITING FOR PATH]
        </div>
        
        <div class="controls">
            <button class="btn-clear" onclick="clearPath()">Clear</button>
            <button class="btn-send" id="sendBtn" onclick="sendToRobot()" disabled>Send to Robot</button>
        </div>
    </div>

    <script>
        const GRID_SIZE = 6;
        const gridElement = document.getElementById('grid');
        const commandOutput = document.getElementById('commandOutput');
        const sendBtn = document.getElementById('sendBtn');
        const statusText = document.getElementById('statusText');
        const statusDot = document.getElementById('statusDot');
        
        let path = [];
        let generatedCommands = ""; 

        function initGrid() {
            gridElement.innerHTML = '';
            gridElement.style.gridTemplateColumns = `repeat(${GRID_SIZE}, 1fr)`;
            
            for (let y = 0; y < GRID_SIZE; y++) {
                for (let x = 0; x < GRID_SIZE; x++) {
                    const cell = document.createElement('div');
                    cell.classList.add('cell');
                    cell.dataset.x = x;
                    cell.dataset.y = y;
                    
                    cell.addEventListener('mousedown', () => handleCellTap(x, y));
                    cell.addEventListener('touchstart', (e) => {
                        e.preventDefault(); 
                        handleCellTap(x, y);
                    });
                    
                    gridElement.appendChild(cell);
                }
            }
        }

        function handleCellTap(x, y) {
            const cellElement = getCellElement(x, y);

            if (path.length === 0) {
                path.push({x, y});
                cellElement.classList.add('active', 'start-node');
                updateDashboard();
                return;
            }

            if (path.some(p => p.x === x && p.y === y)) return; 

            const lastNode = path[path.length - 1];
            const dx = Math.abs(lastNode.x - x);
            const dy = Math.abs(lastNode.y - y);

            if ((dx === 1 && dy === 0) || (dx === 0 && dy === 1)) {
                getCellElement(lastNode.x, lastNode.y).classList.remove('end-node');
                path.push({x, y});
                cellElement.classList.add('active', 'end-node');
                updateDashboard();
            }
        }

        function calculateCommands() {
            if (path.length < 2) return "NOT ENOUGH MOVES";

            let commands = [];
            
            // First, determine the direction of the VERY FIRST move.
            // We will assume the physical robot starts facing this direction.
            let firstDx = path[1].x - path[0].x;
            let firstDy = path[1].y - path[0].y;
            
            let currentDir = 0; // Default UP
            if (firstDy === -1) currentDir = 0;      // UP
            else if (firstDx === 1) currentDir = 1;  // RIGHT
            else if (firstDy === 1) currentDir = 2;  // DOWN
            else if (firstDx === -1) currentDir = 3; // LEFT

            for(let i = 0; i < path.length - 1; i++) {
                let dx = path[i+1].x - path[i].x;
                let dy = path[i+1].y - path[i].y;
                
                let targetDir = 0;
                if (dy === -1) targetDir = 0;      
                else if (dx === 1) targetDir = 1;  
                else if (dy === 1) targetDir = 2;  
                else if (dx === -1) targetDir = 3; 
                
                let turnDiff = (targetDir - currentDir);
                if (turnDiff < 0) turnDiff += 4;
                
                if (turnDiff === 1) commands.push('R');       
                else if (turnDiff === 2) commands.push('U');  
                else if (turnDiff === 3) commands.push('L');  
                
                commands.push('F');
                currentDir = targetDir; 
            }
            
            commands.push('STOP');
            return commands.join(',');
        }

        function updateDashboard() {
            generatedCommands = calculateCommands();
            commandOutput.innerText = generatedCommands;
            
            if (path.length > 1) {
                sendBtn.disabled = false;
                statusText.innerText = "Path Ready";
            }
        }

        function getCellElement(x, y) {
            return document.querySelector(`.cell[data-x="${x}"][data-y="${y}"]`);
        }

        function clearPath() {
            path = [];
            generatedCommands = "";
            commandOutput.innerText = "[WAITING FOR PATH]";
            sendBtn.disabled = true;
            statusText.innerText = "Ready to draw";
            statusDot.className = "status-dot";
            initGrid();
        }

        function sendToRobot() {
            if (!generatedCommands || generatedCommands === "NOT ENOUGH MOVES") return;

            statusText.innerText = "Sending to Robot...";
            statusDot.className = "status-dot";
            
            fetch('/upload-path', {
                method: 'POST',
                headers: { 'Content-Type': 'text/plain' },
                body: generatedCommands
            })
            .then(response => {
                if(response.ok) {
                    statusText.innerText = "Robot Executing!";
                    statusDot.classList.add('connected');
                } else {
                    throw new Error("Server response not OK");
                }
            })
            .catch(error => {
                console.error('Error:', error);
                alert(`[SIMULATION] Sent commands to robot: \n${generatedCommands}\n\n(Note: This is a simulation alert because the ESP32 is not connected yet!)`);
                statusText.innerText = "Simulated Send OK";
                statusDot.classList.add('connected');
            });
        }

        initGrid();
    </script>
</body>
</html>
)rawliteral";

#endif
