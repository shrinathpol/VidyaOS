import React, { useState, useEffect, useRef } from 'react';
import { 
  Terminal as TermIcon, 
  Folder, 
  Cpu, 
  Globe, 
  Settings as SettingsIcon, 
  Sliders, 
  FileText, 
  ShoppingBag, 
  Wifi, 
  Volume2, 
  VolumeX, 
  Bell, 
  Lock, 
  Power, 
  Monitor, 
  ChevronLeft, 
  ChevronRight,
  Maximize2,
  Minimize2,
  X,
  Play
} from 'lucide-react';

// Interfaces
interface AppWindow {
  id: number;
  title: string;
  icon: React.ReactNode;
  open: boolean;
  minimized: boolean;
  x: number;
  y: number;
  w: number;
  h: number;
  workspace: number;
}

interface FileMeta {
  owner: string;
  permissions: number;
}

interface NotificationItem {
  id: number;
  message: string;
  time: string;
}

export default function App() {
  // --- OS Core State ---
  const [theme, setTheme] = useState<'dark' | 'light' | 'neon'>('dark');
  const [activeWorkspace, setActiveWorkspace] = useState<number>(0);
  const [systemLocked, setSystemLocked] = useState<boolean>(false);
  const [passcode, setPasscode] = useState<string>('vidya123');
  const [passcodeInput, setPasscodeInput] = useState<string>('');
  const [currentUser, setCurrentUser] = useState<string>('shri');
  const [users, setUsers] = useState<Record<string, string>>({
    root: 'root123',
    shri: 'vidya123'
  });
  
  // --- Window Manager ---
  const [windows, setWindows] = useState<AppWindow[]>([
    { id: 0, title: 'Terminal', icon: <TermIcon size={14} />, open: false, minimized: false, x: 50, y: 50, w: 480, h: 320, workspace: 0 },
    { id: 1, title: 'File Manager', icon: <Folder size={14} />, open: false, minimized: false, x: 90, y: 90, w: 500, h: 350, workspace: 0 },
    { id: 2, title: 'System Monitor', icon: <Cpu size={14} />, open: false, minimized: false, x: 130, y: 130, w: 420, h: 280, workspace: 0 },
    { id: 3, title: 'Browser', icon: <Globe size={14} />, open: false, minimized: false, x: 170, y: 170, w: 580, h: 400, workspace: 0 },
    { id: 4, title: 'Settings', icon: <SettingsIcon size={14} />, open: false, minimized: false, x: 210, y: 210, w: 460, h: 340, workspace: 0 },
    { id: 5, title: 'Control Panel', icon: <Sliders size={14} />, open: false, minimized: false, x: 250, y: 250, w: 400, h: 300, workspace: 0 },
    { id: 6, title: 'Text Editor', icon: <FileText size={14} />, open: false, minimized: false, x: 290, y: 290, w: 480, h: 330, workspace: 0 },
    { id: 7, title: 'App Store', icon: <ShoppingBag size={14} />, open: false, minimized: false, x: 330, y: 330, w: 520, h: 380, workspace: 0 }
  ]);
  const [zOrder, setZOrder] = useState<number[]>([0, 1, 2, 3, 4, 5, 6, 7]);
  const [draggingId, setDraggingId] = useState<number | null>(null);
  const [dragOffset, setDragOffset] = useState({ x: 0, y: 0 });
  const [startMenuOpen, setStartMenuOpen] = useState(false);
  const [notifPanelOpen, setNotifPanelOpen] = useState(false);
  
  // --- Network State ---
  const [wifiEnabled, setWifiEnabled] = useState(true);
  const [wifiSSID, setWifiSSID] = useState('HomeWiFi');
  const [vpnEnabled, setVpnEnabled] = useState(false);
  const [ipAddress, setIpAddress] = useState('192.168.1.133');
  
  // --- Sound/Notifications State ---
  const [volume, setVolume] = useState(80);
  const [muted, setMuted] = useState(false);
  const [notifications, setNotifications] = useState<NotificationItem[]>([
    { id: 1, message: 'System Boot Completed', time: 'Just Now' }
  ]);
  
  // --- System Metrics ---
  const [cpu, setCpu] = useState(12);
  const [ram, setRam] = useState(2195); // MB
  const [temp, setTemp] = useState(40); // C
  
  // --- Packages ---
  const [installedPkgs, setInstalledPkgs] = useState<Record<string, boolean>>({
    chrome: true,
    neofetch: true,
    cmatrix: false,
    python: false,
    java: false,
    gpp: false,
    gcc: false,
    nodejs: false
  });
  
  // --- Filesystem State ---
  const [virtualFiles, setVirtualFiles] = useState<Record<string, string>>({
    '/etc/vidya/settings.json': JSON.stringify({ theme: 'dark', resolution: '1280x960' }, null, 2),
    '/readme.md': '# VidyaOS Web Simulator\nWelcome to the modern web-based dashboard.',
    '/home/shri/welcome.txt': 'Welcome to VidyaOS!',
    '/home/shri/todo.txt': '- Test cloud sync\n- Check permissions',
    '/var/www/vidyaos.local': '<h1>VidyaOS Intranet</h1><p>Welcome to the secure mock intranet domain.</p><a href="news.local">Read Local News</a>',
    '/var/www/news.local': '<h1>VidyaOS Times</h1><p>Embedded systems build verified. Cloud sync is online.</p><a href="mail.local">Open Mail Client</a>',
    '/var/www/mail.local': '<h1>VidyaOS Mail</h1><p>From: Admin\nSubject: Welcome to Phase 3.</p><a href="vidyaos.local">Return to Intranet Portal</a>'
  });
  
  const [fileMeta, setFileMeta] = useState<Record<string, FileMeta>>({
    '/etc/vidya/settings.json': { owner: 'root', permissions: 0o600 },
    '/readme.md': { owner: 'root', permissions: 0o755 },
    '/home/shri/welcome.txt': { owner: 'shri', permissions: 0o644 },
    '/home/shri/todo.txt': { owner: 'shri', permissions: 0o644 },
    '/var/www/vidyaos.local': { owner: 'root', permissions: 0o755 }
  });

  const [activeFile, setActiveFile] = useState<string>('/home/shri/welcome.txt');

  // --- Browser Navigation State ---
  const [browserUrl, setBrowserUrl] = useState('vidyaos.local');
  const [browserHistory, setBrowserHistory] = useState<string[]>(['vidyaos.local']);
  const [browserHistoryIdx, setBrowserHistoryIdx] = useState(0);

  // --- Firewall State ---
  const [firewallRules, setFirewallRules] = useState<Record<string, boolean>>({
    Terminal: true,
    Browser: true,
    'App Store': true
  });

  // Dynamic Telemetry loop
  useEffect(() => {
    const interval = setInterval(() => {
      setCpu(Math.floor(Math.random() * 25) + 5);
      setRam(prev => {
        const delta = Math.floor(Math.random() * 21) - 10;
        return Math.max(1024, Math.min(4096, prev + delta));
      });
      setTemp(Math.floor(Math.random() * 6) + 38);
    }, 1500);
    return () => clearInterval(interval);
  }, []);

  const addNotification = (message: string) => {
    setNotifications(prev => [
      { id: Date.now(), message, time: new Date().toLocaleTimeString() },
      ...prev.slice(0, 4)
    ]);
  };

  // Window Management Actions
  const openWindow = (id: number) => {
    setWindows(prev => prev.map(w => w.id === id ? { ...w, open: true, minimized: false, workspace: activeWorkspace } : w));
    focusWindow(id);
    setStartMenuOpen(false);
  };

  const closeWindow = (id: number) => {
    setWindows(prev => prev.map(w => w.id === id ? { ...w, open: false } : w));
  };

  const minimizeWindow = (id: number) => {
    setWindows(prev => prev.map(w => w.id === id ? { ...w, minimized: true } : w));
  };

  const focusWindow = (id: number) => {
    setZOrder(prev => {
      const filtered = prev.filter(x => x !== id);
      return [...filtered, id];
    });
  };

  const toggleWindow = (id: number) => {
    const target = windows.find(w => w.id === id);
    if (!target) return;
    if (target.open) {
      if (target.minimized || zOrder[zOrder.length - 1] !== id) {
        setWindows(prev => prev.map(w => w.id === id ? { ...w, minimized: false } : w));
        focusWindow(id);
      } else {
        minimizeWindow(id);
      }
    } else {
      openWindow(id);
    }
  };

  // Window Drag Helpers
  const handleMouseDown = (e: React.MouseEvent, id: number) => {
    focusWindow(id);
    const win = windows.find(w => w.id === id);
    if (!win) return;
    setDraggingId(id);
    setDragOffset({
      x: e.clientX - win.x,
      y: e.clientY - win.y
    });
  };

  const handleMouseMove = (e: MouseEvent) => {
    if (draggingId === null) return;
    setWindows(prev => prev.map(w => w.id === draggingId ? {
      ...w,
      x: Math.max(0, Math.min(window.innerWidth - 100, e.clientX - dragOffset.x)),
      y: Math.max(0, Math.min(window.innerHeight - 45 - 36, e.clientY - dragOffset.y))
    } : w));
  };

  const handleMouseUp = () => {
    setDraggingId(null);
  };

  useEffect(() => {
    window.addEventListener('mousemove', handleMouseMove);
    window.addEventListener('mouseup', handleMouseUp);
    return () => {
      window.removeEventListener('mousemove', handleMouseMove);
      window.removeEventListener('mouseup', handleMouseUp);
    };
  }, [draggingId, dragOffset]);

  const handleUnlock = () => {
    if (passcodeInput === passcode) {
      setSystemLocked(false);
      setPasscodeInput('');
      addNotification('Console Unlocked');
    } else {
      alert('Incorrect Passcode');
      setPasscodeInput('');
    }
  };

  // Render components
  return (
    <div className="w-full h-full relative" data-theme={theme} style={{ overflow: 'hidden' }}>
      
      {/* --- Lock Screen overlay --- */}
      {systemLocked && (
        <div className="absolute inset-0 z-[9999] flex flex-col items-center justify-center bg-black/85 backdrop-blur-2xl text-white font-sans animate-fade-in">
          <div className="glass p-8 rounded-2xl flex flex-col items-center max-w-sm w-full text-center border-white/10 shadow-2xl">
            <Lock size={48} className="text-cyan-400 animate-pulse mb-4" />
            <h1 className="text-2xl font-bold mb-6 font-display">VidyaOS Secure Lock</h1>
            <p className="text-slate-400 text-sm mb-4">Enter user passcode to gain access</p>
            <input 
              type="password" 
              className="bg-black/50 border border-white/10 rounded-lg px-4 py-2 text-center w-full mb-4 tracking-widest text-cyan-400 font-mono"
              value={passcodeInput}
              onChange={(e) => setPasscodeInput(e.target.value)}
              onKeyDown={(e) => e.key === 'Enter' && handleUnlock()}
              placeholder="••••••••"
              autoFocus
            />
            <button 
              onClick={handleUnlock}
              className="w-full bg-cyan-500 hover:bg-cyan-600 active:scale-[0.98] transition-transform text-black py-2 rounded-lg font-semibold flex items-center justify-center gap-2"
            >
              Unlock <ChevronRight size={16} />
            </button>
          </div>
        </div>
      )}

      {/* --- Desktop Icons Grid --- */}
      <div className="desktop-grid">
        <div className="desktop-icon" onDoubleClick={() => openWindow(0)}>
          <div className="desktop-icon-image"><TermIcon size={32} /></div>
          <div className="desktop-icon-label text-slate-100">Terminal</div>
        </div>
        <div className="desktop-icon" onDoubleClick={() => openWindow(1)}>
          <div className="desktop-icon-image"><Folder size={32} /></div>
          <div className="desktop-icon-label text-slate-100">Files</div>
        </div>
        <div className="desktop-icon" onDoubleClick={() => openWindow(2)}>
          <div className="desktop-icon-image"><Cpu size={32} /></div>
          <div className="desktop-icon-label text-slate-100">Monitor</div>
        </div>
        {installedPkgs.chrome && (
          <div className="desktop-icon" onDoubleClick={() => openWindow(3)}>
            <div className="desktop-icon-image text-emerald-400"><Globe size={32} /></div>
            <div className="desktop-icon-label text-slate-100">Browser</div>
          </div>
        )}
        <div className="desktop-icon" onDoubleClick={() => openWindow(4)}>
          <div className="desktop-icon-image text-cyan-400"><SettingsIcon size={32} /></div>
          <div className="desktop-icon-label text-slate-100">Settings</div>
        </div>
        <div className="desktop-icon" onDoubleClick={() => openWindow(5)}>
          <div className="desktop-icon-image text-purple-400"><Sliders size={32} /></div>
          <div className="desktop-icon-label text-slate-100">Panel</div>
        </div>
        <div className="desktop-icon" onDoubleClick={() => openWindow(6)}>
          <div className="desktop-icon-image text-amber-400"><FileText size={32} /></div>
          <div className="desktop-icon-label text-slate-100">Editor</div>
        </div>
        <div className="desktop-icon" onDoubleClick={() => openWindow(7)}>
          <div className="desktop-icon-image text-orange-400"><ShoppingBag size={32} /></div>
          <div className="desktop-icon-label text-slate-100">Store</div>
        </div>
      </div>

      {/* --- Windows Compositor Layer --- */}
      {zOrder.map(id => {
        const win = windows.find(w => w.id === id);
        if (!win || !win.open || win.workspace !== activeWorkspace) return null;
        const focused = zOrder[zOrder.length - 1] === id;
        
        return (
          <div 
            key={id}
            onClick={() => focusWindow(id)}
            className={`app-window glass ${win.minimized ? 'hidden' : ''} ${focused ? 'ring-1 ring-cyan-500/30' : ''}`}
            style={{ 
              left: `${win.x}px`, 
              top: `${win.y}px`, 
              width: `${win.w}px`, 
              height: `${win.h}px`,
              zIndex: zOrder.indexOf(id) + 10 
            }}
          >
            {/* Window title bar */}
            <div 
              className="window-titlebar"
              onMouseDown={(e) => handleMouseDown(e, id)}
            >
              <div className="window-title">
                {win.icon} {win.title}
              </div>
              <div className="window-controls">
                <button 
                  onClick={(e) => { e.stopPropagation(); minimizeWindow(id); }}
                  className="window-control-btn minimize"
                />
                <button 
                  onClick={(e) => { e.stopPropagation(); closeWindow(id); }}
                  className="window-control-btn close"
                />
              </div>
            </div>
            {/* Window app content */}
            <div className="window-content flex flex-col h-full bg-slate-950/80">
              {id === 0 && <TerminalApp user={currentUser} files={virtualFiles} setFiles={setVirtualFiles} setLocked={setSystemLocked} addNotif={addNotification} setWorkspace={setActiveWorkspace} setTheme={setTheme} fileMeta={fileMeta} setFileMeta={setFileMeta} users={users} setUsers={setUsers} setCurrentUser={setCurrentUser} />}
              {id === 1 && <FileManagerApp files={virtualFiles} setFiles={setVirtualFiles} openTextEditor={() => openWindow(6)} activeFile={activeFile} setActiveFile={setActiveFile} fileMeta={fileMeta} currentUser={currentUser} addNotif={addNotification} />}
              {id === 2 && <SystemMonitorApp cpu={cpu} ram={ram} temp={temp} />}
              {id === 3 && <BrowserApp files={virtualFiles} url={browserUrl} setUrl={setBrowserUrl} history={browserHistory} setHistory={setBrowserHistory} historyIdx={browserHistoryIdx} setHistoryIdx={setBrowserHistoryIdx} />}
              {id === 4 && <SettingsApp theme={theme} setTheme={setTheme} volume={volume} setVolume={setVolume} muted={muted} setMuted={setMuted} wifiEnabled={wifiEnabled} setWifiEnabled={setWifiEnabled} vpnEnabled={vpnEnabled} setVpnEnabled={setVpnEnabled} ip={ipAddress} setPasscode={setPasscode} currentUser={currentUser} />}
              {id === 5 && <ControlPanelApp rules={firewallRules} setRules={setFirewallRules} />}
              {id === 6 && <TextEditorApp files={virtualFiles} setFiles={setVirtualFiles} activeFile={activeFile} setActiveFile={setActiveFile} />}
              {id === 7 && <AppStoreApp installed={installedPkgs} setInstalled={setInstalledPkgs} addNotif={addNotification} />}
            </div>
          </div>
        );
      })}

      {/* --- HUD widget --- */}
      <div className="absolute top-5 right-5 z-0 glass p-4 rounded-xl flex flex-col gap-2 min-w-[150px] border-white/5 font-sans pointer-events-none select-none">
        <div className="text-[11px] text-slate-400 font-bold uppercase tracking-wider mb-1 flex items-center gap-1"><Monitor size={12} /> HUD Widget</div>
        <div className="flex justify-between items-center text-xs">
          <span className="text-slate-400">CPU Usage</span>
          <span className="font-mono font-bold text-cyan-400">{cpu}%</span>
        </div>
        <div className="w-full h-1 bg-white/5 rounded-full overflow-hidden">
          <div className="h-full bg-cyan-400 transition-all duration-300" style={{ width: `${cpu}%` }} />
        </div>
        
        <div className="flex justify-between items-center text-xs mt-1">
          <span className="text-slate-400">RAM Allocated</span>
          <span className="font-mono font-bold text-cyan-400">{ram}M</span>
        </div>
        <div className="w-full h-1 bg-white/5 rounded-full overflow-hidden">
          <div className="h-full bg-cyan-400 transition-all duration-300" style={{ width: `${(ram / 4096) * 100}%` }} />
        </div>

        <div className="flex justify-between items-center text-xs mt-1">
          <span className="text-slate-400">Core Temp</span>
          <span className="font-mono font-bold text-cyan-400">{temp}°C</span>
        </div>
      </div>

      {/* --- Floating Notifications overlay panel --- */}
      {notifPanelOpen && (
        <div className="absolute bottom-[50px] right-3 z-[999] glass w-72 rounded-xl p-4 border-white/10 font-sans shadow-2xl">
          <div className="flex justify-between items-center border-b border-white/5 pb-2 mb-3">
            <span className="text-xs font-bold text-slate-400 uppercase tracking-wider flex items-center gap-1"><Bell size={12} /> Notifications</span>
            <button onClick={() => setNotifications([])} className="text-[10px] text-cyan-400 hover:underline">Clear All</button>
          </div>
          <div className="flex flex-col gap-2 max-h-48 overflow-y-auto">
            {notifications.length === 0 ? (
              <div className="text-xs text-slate-500 text-center py-4">No recent logs</div>
            ) : (
              notifications.map(n => (
                <div key={n.id} className="text-xs bg-white/5 p-2 rounded border border-white/5 flex flex-col gap-1">
                  <span className="text-slate-200">{n.message}</span>
                  <span className="text-[9px] text-slate-500 align-right self-end">{n.time}</span>
                </div>
              ))
            )}
          </div>
        </div>
      )}

      {/* --- Start Menu Panel --- */}
      {startMenuOpen && (
        <div className="absolute bottom-[50px] left-3 z-[999] glass w-64 rounded-xl p-4 border-white/10 font-sans shadow-2xl flex flex-col gap-3">
          <div className="flex items-center gap-3 border-b border-white/5 pb-3">
            <div className="w-10 h-10 rounded-full bg-cyan-500/20 border border-cyan-500/30 flex items-center justify-center font-bold text-cyan-400">
              {currentUser[0].toUpperCase()}
            </div>
            <div>
              <div className="text-xs font-bold text-slate-200">{currentUser}</div>
              <div className="text-[10px] text-slate-500">Virtual Session</div>
            </div>
          </div>
          
          <div className="flex flex-col gap-1">
            <button onClick={() => openWindow(0)} className="w-full text-left py-2 px-3 hover:bg-white/5 rounded text-xs flex items-center gap-2"><TermIcon size={14} className="text-cyan-400" /> Terminal</button>
            <button onClick={() => openWindow(1)} className="w-full text-left py-2 px-3 hover:bg-white/5 rounded text-xs flex items-center gap-2"><Folder size={14} className="text-cyan-400" /> File Explorer</button>
            <button onClick={() => openWindow(2)} className="w-full text-left py-2 px-3 hover:bg-white/5 rounded text-xs flex items-center gap-2"><Cpu size={14} className="text-cyan-400" /> System Monitor</button>
            <button onClick={() => openWindow(3)} className="w-full text-left py-2 px-3 hover:bg-white/5 rounded text-xs flex items-center gap-2"><Globe size={14} className="text-cyan-400" /> Web Browser</button>
            <button onClick={() => openWindow(4)} className="w-full text-left py-2 px-3 hover:bg-white/5 rounded text-xs flex items-center gap-2"><SettingsIcon size={14} className="text-cyan-400" /> Settings</button>
            <button onClick={() => openWindow(5)} className="w-full text-left py-2 px-3 hover:bg-white/5 rounded text-xs flex items-center gap-2"><Sliders size={14} className="text-cyan-400" /> Control Panel</button>
          </div>
          
          <div className="flex justify-between items-center border-t border-white/5 pt-3 mt-1">
            <button 
              onClick={() => { setSystemLocked(true); setStartMenuOpen(false); }}
              className="text-xs hover:bg-white/5 p-1 px-2 rounded flex items-center gap-1 text-slate-400 hover:text-white"
            >
              <Lock size={12} /> Lock
            </button>
            <button 
              onClick={() => { if(confirm('Shutdown VM simulator?')) window.close(); }}
              className="text-xs hover:bg-white/5 p-1 px-2 rounded flex items-center gap-1 text-red-400 hover:text-red-300"
            >
              <Power size={12} /> Shut Down
            </button>
          </div>
        </div>
      )}

      {/* --- Taskbar 3.0 (Bottom edge) --- */}
      <div className="absolute bottom-0 left-0 right-0 z-[1000] h-[45px] glass border-t border-white/10 flex items-center justify-between px-3 select-none">
        
        {/* Left: Start button & workspaces */}
        <div className="flex items-center gap-3">
          <button 
            onClick={() => setStartMenuOpen(!startMenuOpen)}
            className="h-8 px-4 rounded-lg bg-cyan-500/20 hover:bg-cyan-500/30 text-cyan-400 text-xs font-bold font-display border border-cyan-500/30 hover:scale-[1.02] transition-transform active:scale-[0.98]"
          >
            Vidya Menu
          </button>
          
          {/* Workspaces Switcher */}
          <div className="flex items-center gap-1 bg-white/5 p-1 rounded-lg border border-white/5">
            {[0, 1, 2, 3].map(w => (
              <button 
                key={w}
                onClick={() => setActiveWorkspace(w)}
                className={`w-6 h-6 rounded-md text-[10px] font-bold transition-all ${activeWorkspace === w ? 'bg-cyan-400 text-black shadow' : 'text-slate-400 hover:bg-white/5'}`}
              >
                {w + 1}
              </button>
            ))}
          </div>
        </div>

        {/* Center: Running tasks bar */}
        <div className="flex items-center gap-2 overflow-x-auto max-w-[50%] px-2">
          {windows.map(w => {
            if (!w.open || w.workspace !== activeWorkspace) return null;
            const focused = zOrder[zOrder.length - 1] === w.id;
            return (
              <button 
                key={w.id}
                onClick={() => toggleWindow(w.id)}
                className={`h-8 px-3 rounded-lg text-xs font-semibold flex items-center gap-2 border transition-all ${focused ? 'bg-cyan-500/20 text-cyan-400 border-cyan-500/40' : 'bg-white/5 text-slate-300 border-white/5 hover:bg-white/10'}`}
              >
                {w.icon}
                <span className="max-w-[80px] truncate">{w.title}</span>
              </button>
            );
          })}
        </div>

        {/* Right: Trays (Wifi, Volume, Bell, Clock) */}
        <div className="flex items-center gap-4">
          <div className="flex items-center gap-2 text-xs">
            {wifiEnabled ? (
              <div className="flex items-center gap-1 text-emerald-400">
                <Wifi size={14} />
                <span className="text-[10px] text-slate-400 truncate max-w-[60px]">{wifiSSID}</span>
              </div>
            ) : (
              <Wifi size={14} className="text-slate-500" />
            )}
            
            <button onClick={() => setMuted(!muted)} className="text-slate-300 hover:text-white">
              {muted || volume === 0 ? <VolumeX size={14} className="text-red-400" /> : <Volume2 size={14} />}
            </button>
          </div>

          <button 
            onClick={() => setNotifPanelOpen(!notifPanelOpen)}
            className="relative p-1 text-slate-300 hover:text-white"
          >
            <Bell size={14} />
            {notifications.length > 0 && (
              <div className="absolute top-0 right-0 w-2 h-2 rounded-full bg-red-500 animate-ping" />
            )}
          </button>

          <TaskbarClock />
        </div>

      </div>

    </div>
  );
}

// Subcomponents: Clock
function TaskbarClock() {
  const [time, setTime] = useState('');
  useEffect(() => {
    const updateTime = () => {
      setTime(new Date().toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: '2-digit' }));
    };
    updateTime();
    const interval = setInterval(updateTime, 1000);
    return () => clearInterval(interval);
  }, []);
  return (
    <span className="text-xs font-mono font-bold text-slate-300 select-none">
      {time}
    </span>
  );
}

// -------------------------------------------------------------
//  Subcomponents: App Implementations
// -------------------------------------------------------------

// 1. Terminal App
interface TerminalProps {
  user: string;
  files: Record<string, string>;
  setFiles: React.Dispatch<React.SetStateAction<Record<string, string>>>;
  setLocked: React.Dispatch<React.SetStateAction<boolean>>;
  addNotif: (message: string) => void;
  setWorkspace: React.Dispatch<React.SetStateAction<number>>;
  setTheme: React.Dispatch<React.SetStateAction<'dark' | 'light' | 'neon'>>;
  fileMeta: Record<string, FileMeta>;
  setFileMeta: React.Dispatch<React.SetStateAction<Record<string, FileMeta>>>;
  users: Record<string, string>;
  setUsers: React.Dispatch<React.SetStateAction<Record<string, string>>>;
  setCurrentUser: React.Dispatch<React.SetStateAction<string>>;
}

function TerminalApp({ 
  user, files, setFiles, setLocked, addNotif, 
  setWorkspace, setTheme, fileMeta, setFileMeta, users, setUsers, setCurrentUser 
}: TerminalProps) {
  const [history, setHistory] = useState<{ type: 'in' | 'out', val: string }[]>([
    { type: 'out', val: 'VidyaOS Console Shell v3.0' },
    { type: 'out', val: 'Type "help" for a list of simulated commands.' }
  ]);
  const [cmd, setCmd] = useState('');
  const bottomRef = useRef<HTMLDivElement | null>(null);

  useEffect(() => {
    bottomRef.current?.scrollIntoView({ behavior: 'smooth' });
  }, [history]);

  const executeCommand = (input: string) => {
    const trimmed = input.trim();
    if (!trimmed) return;
    
    const parts = trimmed.split(/\s+/);
    const command = parts[0];
    const args = parts.slice(1);
    
    let response = '';
    
    switch (command) {
      case 'help':
        response = `Available commands:
  help                    - Display this reference card
  ls                      - List files in current directory
  cat [file]              - Print file content
  echo [text] > [file]    - Write file contents
  rm [file]               - Delete a virtual file
  whoami                  - Print active user
  user switch [user]      - Switch user session
  user add [user] [pass]  - Register new system user
  user list               - List all registered users
  network status          - Display mock Wi-Fi telemetry
  network connect [ssid]  - Connect to local mock SSID
  workspace [0-3]         - Shift visual workspaces
  theme apply [name]      - Shift UI skin (dark, light, neon)
  chmod [octal] [file]    - Change file access flags
  chown [owner] [file]    - Change file owner
  lock                    - Trigger screensaver session
  clear                   - Clear screen buffer`;
        break;
      case 'whoami':
        response = user;
        break;
      case 'ls':
        response = Object.keys(files).join('\n');
        break;
      case 'cat':
        if (!args[0]) response = 'Usage: cat [file_path]';
        else if (files[args[0]] !== undefined) response = files[args[0]];
        else response = `cat: ${args[0]}: No such file or directory`;
        break;
      case 'echo':
        const redirectIdx = parts.indexOf('>');
        if (redirectIdx !== -1 && parts[redirectIdx + 1]) {
          const text = parts.slice(1, redirectIdx).join(' ').replace(/"/g, '');
          const path = parts[redirectIdx + 1];
          setFiles(prev => ({ ...prev, [path]: text }));
          if (!fileMeta[path]) {
            setFileMeta(prev => ({ ...prev, [path]: { owner: user, permissions: 0o644 } }));
          }
          response = `Wrote to ${path}`;
        } else {
          response = args.join(' ').replace(/"/g, '');
        }
        break;
      case 'rm':
        if (!args[0]) response = 'Usage: rm [file]';
        else if (files[args[0]] !== undefined) {
          setFiles(prev => {
            const copy = { ...prev };
            delete copy[args[0]];
            return copy;
          });
          response = `Deleted ${args[0]}`;
        } else response = `rm: ${args[0]}: No such file`;
        break;
      case 'lock':
        setLocked(true);
        response = 'Console locked';
        break;
      case 'workspace':
        const ws = parseInt(args[0]);
        if (ws >= 0 && ws <= 3) {
          setWorkspace(ws);
          addNotif(`Workspace switched to ${ws}`);
          response = `Workspace shifted to ${ws}`;
        } else response = 'Usage: workspace [0-3]';
        break;
      case 'theme':
        if (args[0] === 'apply' && (args[1] === 'dark' || args[1] === 'light' || args[1] === 'neon')) {
          setTheme(args[1] as any);
          addNotif(`Theme applied: ${args[1]}`);
          response = `Skin set to ${args[1]}`;
        } else response = 'Usage: theme apply [dark|light|neon]';
        break;
      case 'user':
        if (args[0] === 'list') {
          response = Object.keys(users).join('\n');
        } else if (args[0] === 'add') {
          if (user !== 'root') response = 'Permission denied: root only';
          else if (!args[1] || !args[2]) response = 'Usage: user add [username] [passcode]';
          else {
            setUsers(prev => ({ ...prev, [args[1]]: args[2] }));
            response = `User "${args[1]}" created.`;
          }
        } else if (args[0] === 'switch') {
          if (!args[1]) response = 'Usage: user switch [username]';
          else if (!users[args[1]]) response = `user: ${args[1]}: User not found`;
          else {
            const pass = prompt(`Enter passcode for ${args[1]}:`);
            if (pass === users[args[1]]) {
              setCurrentUser(args[1]);
              response = `Switched to user: ${args[1]}`;
              addNotif(`User session switched to ${args[1]}`);
            } else {
              response = 'Authentication failed: incorrect passcode';
            }
          }
        } else response = 'Usage: user <list|add|switch>';
        break;
      case 'network':
        if (args[0] === 'status') {
          response = `SSID: HomeWiFi\nSignal: Excellent\nIP: 192.168.1.133\nVPN: Disabled`;
        } else if (args[0] === 'connect') {
          response = `Connected to SSID: ${args[1] || 'Guest WiFi'}`;
        } else response = 'Usage: network <status|connect>';
        break;
      case 'chmod':
        if (!args[0] || !args[1]) response = 'Usage: chmod [octal] [file]';
        else if (!files[args[1]]) response = `chmod: ${args[1]}: File not found`;
        else {
          const mode = parseInt(args[0], 8);
          setFileMeta(prev => ({
            ...prev,
            [args[1]]: { ...prev[args[1]], permissions: mode }
          }));
          response = `Permissions updated to ${args[0]} for ${args[1]}`;
        }
        break;
      case 'chown':
        if (!args[0] || !args[1]) response = 'Usage: chown [owner] [file]';
        else if (!files[args[1]]) response = `chown: ${args[1]}: File not found`;
        else {
          setFileMeta(prev => ({
            ...prev,
            [args[1]]: { ...prev[args[1]], owner: args[0] }
          }));
          response = `Ownership set to ${args[0]} for ${args[1]}`;
        }
        break;
      case 'clear':
        setHistory([]);
        setCmd('');
        return;
      default:
        response = `Command "${command}" not found. Type "help" for options.`;
    }

    setHistory(prev => [
      ...prev,
      { type: 'in', val: `${user}@vidyaos:~$ ${input}` },
      { type: 'out', val: response }
    ]);
    setCmd('');
  };

  return (
    <div className="flex-1 flex flex-col p-3 font-mono text-xs overflow-hidden h-full text-emerald-400 select-text">
      <div className="flex-1 overflow-y-auto flex flex-col gap-1 pr-1">
        {history.map((h, i) => (
          <div key={i} className={h.type === 'in' ? 'text-cyan-400' : 'text-emerald-400 whitespace-pre-wrap'}>
            {h.val}
          </div>
        ))}
        <div ref={bottomRef} />
      </div>
      <div className="flex items-center gap-2 border-t border-white/5 pt-2 mt-2">
        <span className="text-cyan-400">{user}@vidyaos:~$</span>
        <input 
          type="text" 
          value={cmd}
          onChange={(e) => setCmd(e.target.value)}
          onKeyDown={(e) => e.key === 'Enter' && executeCommand(cmd)}
          className="flex-1 bg-transparent border-none text-emerald-400 font-mono text-xs focus:ring-0 p-0"
          placeholder="Type command..."
          autoFocus
        />
      </div>
    </div>
  );
}

// 2. File Manager App
interface FileManagerProps {
  files: Record<string, string>;
  setFiles: React.Dispatch<React.SetStateAction<Record<string, string>>>;
  openTextEditor: () => void;
  activeFile: string;
  setActiveFile: (file: string) => void;
  fileMeta: Record<string, FileMeta>;
  currentUser: string;
  addNotif: (msg: string) => void;
}

function FileManagerApp({ files, setFiles, openTextEditor, activeFile, setActiveFile, fileMeta, currentUser, addNotif }: FileManagerProps) {
  const [currentFolder, setCurrentFolder] = useState<string>('/home/shri');
  const [contextMenu, setContextMenu] = useState<{ x: number, y: number, target: string } | null>(null);

  const folders = ['/', '/home', '/home/shri', '/etc', '/etc/vidya', '/var', '/var/www'];
  
  const getFilesForFolder = () => {
    return Object.keys(files).filter(path => {
      const idx = path.lastIndexOf('/');
      const parent = idx === 0 ? '/' : path.substring(0, idx);
      return parent === currentFolder;
    });
  };

  const handleFileDoubleClick = (path: string) => {
    const meta = fileMeta[path];
    if (meta && meta.owner !== currentUser && currentUser !== 'root') {
      // Permission check (simplified)
      alert(`Permission Denied: owned by ${meta.owner}`);
      return;
    }
    setActiveFile(path);
    openTextEditor();
  };

  const handleRightClick = (e: React.MouseEvent, path: string) => {
    e.preventDefault();
    setContextMenu({
      x: e.clientX,
      y: e.clientY,
      target: path
    });
  };

  const handleDeleteFile = () => {
    if (!contextMenu) return;
    const path = contextMenu.target;
    const meta = fileMeta[path];
    if (meta && meta.owner !== currentUser && currentUser !== 'root') {
      alert('Permission Denied');
      setContextMenu(null);
      return;
    }

    setFiles(prev => {
      const copy = { ...prev };
      delete copy[path];
      return copy;
    });
    addNotif(`Deleted file: ${path}`);
    setContextMenu(null);
  };

  const handleCreateFile = () => {
    const name = prompt('Enter file name:');
    if (!name) return;
    const fullPath = `${currentFolder === '/' ? '' : currentFolder}/${name}`;
    setFiles(prev => ({ ...prev, [fullPath]: 'New File' }));
    addNotif(`Created file: ${fullPath}`);
  };

  useEffect(() => {
    const closeMenu = () => setContextMenu(null);
    window.addEventListener('click', closeMenu);
    return () => window.removeEventListener('click', closeMenu);
  }, []);

  return (
    <div className="flex-1 flex overflow-hidden text-slate-200 font-sans text-xs h-full relative">
      {/* Sidebar folders */}
      <div className="w-[150px] border-r border-white/5 bg-black/20 flex flex-col p-2 gap-1 overflow-y-auto">
        <span className="text-[10px] text-slate-500 font-bold tracking-wider mb-2 uppercase">Directories</span>
        {folders.map(f => (
          <button 
            key={f}
            onClick={() => setCurrentFolder(f)}
            className={`w-full text-left px-2 py-1.5 rounded transition-all flex items-center gap-2 ${currentFolder === f ? 'bg-cyan-500/20 text-cyan-400 font-semibold' : 'hover:bg-white/5'}`}
          >
            <Folder size={14} className={currentFolder === f ? 'text-cyan-400' : 'text-slate-400'} />
            <span className="truncate">{f}</span>
          </button>
        ))}
      </div>
      
      {/* Main Files Listing */}
      <div className="flex-1 flex flex-col p-4 overflow-y-auto">
        <div className="flex justify-between items-center mb-4">
          <span className="text-slate-400">Path: <span className="font-mono text-cyan-400">{currentFolder}</span></span>
          <button 
            onClick={handleCreateFile}
            className="bg-cyan-500/20 hover:bg-cyan-500/30 text-cyan-400 border border-cyan-500/30 px-3 py-1 rounded text-[11px]"
          >
            + New File
          </button>
        </div>
        
        <div className="grid grid-cols-3 gap-3">
          {getFilesForFolder().map(path => {
            const filename = path.substring(path.lastIndexOf('/') + 1);
            return (
              <div 
                key={path}
                onDoubleClick={() => handleFileDoubleClick(path)}
                onContextMenu={(e) => handleRightClick(e, path)}
                className="glass p-3 rounded-lg flex items-center gap-3 border-white/5 cursor-pointer hover:border-cyan-500/30 transition-all hover:bg-white/5"
              >
                <FileText className="text-amber-400 shrink-0" size={24} />
                <div className="flex flex-col overflow-hidden">
                  <span className="font-medium truncate text-slate-200">{filename}</span>
                  <span className="text-[10px] text-slate-500 font-mono">{(files[path].length)} bytes</span>
                </div>
              </div>
            );
          })}
        </div>
      </div>

      {/* Context Menu */}
      {contextMenu && (
        <div 
          className="absolute z-[9999] glass border-white/10 w-28 rounded-lg p-1 shadow-2xl flex flex-col gap-0.5"
          style={{ left: `${contextMenu.x - 170}px`, top: `${contextMenu.y - 120}px` }}
        >
          <button 
            onClick={() => handleFileDoubleClick(contextMenu.target)}
            className="w-full text-left hover:bg-white/5 p-1.5 rounded text-[11px]"
          >
            Edit
          </button>
          <button 
            onClick={handleDeleteFile}
            className="w-full text-left text-red-400 hover:bg-red-500/10 p-1.5 rounded text-[11px]"
          >
            Delete
          </button>
        </div>
      )}
    </div>
  );
}

// 3. System Monitor App
function SystemMonitorApp({ cpu, ram, temp }: { cpu: number, ram: number, temp: number }) {
  const canvasRef = useRef<HTMLCanvasElement | null>(null);
  const [history, setHistory] = useState<number[]>(new Array(30).fill(0));

  useEffect(() => {
    setHistory(prev => [...prev.slice(1), cpu]);
  }, [cpu]);

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    ctx.clearRect(0, 0, canvas.width, canvas.height);
    
    // Draw grid
    ctx.strokeStyle = 'rgba(255,255,255,0.03)';
    ctx.lineWidth = 1;
    for (let x = 0; x < canvas.width; x += 20) {
      ctx.beginPath();
      ctx.moveTo(x, 0);
      ctx.lineTo(x, canvas.height);
      ctx.stroke();
    }
    for (let y = 0; y < canvas.height; y += 20) {
      ctx.beginPath();
      ctx.moveTo(0, y);
      ctx.lineTo(canvas.width, y);
      ctx.stroke();
    }

    // Draw wave line
    ctx.strokeStyle = '#00bfff';
    ctx.shadowColor = '#00bfff';
    ctx.shadowBlur = 6;
    ctx.lineWidth = 2;
    ctx.beginPath();

    const dx = canvas.width / (history.length - 1);
    history.forEach((val, idx) => {
      const x = idx * dx;
      const y = canvas.height - (val / 100) * canvas.height;
      if (idx === 0) ctx.moveTo(x, y);
      else ctx.lineTo(x, y);
    });
    ctx.stroke();
    ctx.shadowBlur = 0; // reset
  }, [history]);

  return (
    <div className="flex-1 flex flex-col p-4 text-slate-300 font-sans text-xs h-full gap-4">
      <div className="grid grid-cols-3 gap-3">
        <div className="glass p-3 rounded-lg border-white/5 flex flex-col">
          <span className="text-[10px] text-slate-500 font-bold uppercase tracking-wider">CPU Core</span>
          <span className="text-xl font-bold text-cyan-400 font-mono mt-1">{cpu}%</span>
        </div>
        <div className="glass p-3 rounded-lg border-white/5 flex flex-col">
          <span className="text-[10px] text-slate-500 font-bold uppercase tracking-wider">RAM Usage</span>
          <span className="text-xl font-bold text-cyan-400 font-mono mt-1">{ram} MB</span>
        </div>
        <div className="glass p-3 rounded-lg border-white/5 flex flex-col">
          <span className="text-[10px] text-slate-500 font-bold uppercase tracking-wider">Processor Temp</span>
          <span className="text-xl font-bold text-cyan-400 font-mono mt-1">{temp}°C</span>
        </div>
      </div>
      
      <div className="flex-1 bg-black/45 rounded-lg border border-white/5 relative p-2 overflow-hidden flex flex-col">
        <span className="text-[9px] text-slate-500 font-semibold mb-2">Real-time Performance Graph</span>
        <canvas ref={canvasRef} className="w-full flex-1" width={380} height={130} />
      </div>
    </div>
  );
}

// 4. Browser App
interface BrowserProps {
  files: Record<string, string>;
  url: string;
  setUrl: (u: string) => void;
  history: string[];
  setHistory: React.Dispatch<React.SetStateAction<string[]>>;
  historyIdx: number;
  setHistoryIdx: (idx: number) => void;
}

function BrowserApp({ files, url, setUrl, history, setHistory, historyIdx, setHistoryIdx }: BrowserProps) {
  const [navInput, setNavInput] = useState(url);

  const navigateTo = (dest: string) => {
    const cleanUrl = dest.trim();
    if (!cleanUrl) return;
    
    const nextHistory = history.slice(0, historyIdx + 1);
    setHistory([...nextHistory, cleanUrl]);
    setHistoryIdx(nextHistory.length);
    setUrl(cleanUrl);
    setNavInput(cleanUrl);
  };

  const handleBack = () => {
    if (historyIdx > 0) {
      setHistoryIdx(historyIdx - 1);
      setUrl(history[historyIdx - 1]);
      setNavInput(history[historyIdx - 1]);
    }
  };

  const handleForward = () => {
    if (historyIdx < history.length - 1) {
      setHistoryIdx(historyIdx + 1);
      setUrl(history[historyIdx + 1]);
      setNavInput(history[historyIdx + 1]);
    }
  };

  const renderWebpageContent = () => {
    const filepath = `/var/www/${url}`;
    if (files[filepath]) {
      return (
        <div 
          className="p-6 font-sans text-slate-300 flex flex-col gap-4 select-text"
          dangerouslySetInnerHTML={{ __html: files[filepath] }}
        />
      );
    }
    
    // Default 404 / Mock Search landing page
    return (
      <div className="p-8 font-sans flex flex-col items-center justify-center text-center">
        <h1 className="text-3xl font-extrabold text-cyan-400 font-display mb-6">VidyaSearch</h1>
        <div className="glass max-w-md w-full p-6 rounded-2xl border-white/10 shadow-xl mb-4 flex flex-col gap-4">
          <input 
            type="text" 
            placeholder="Search virtual portal..."
            className="w-full bg-black/20 border border-white/10 rounded-lg px-4 py-2 text-center text-xs text-cyan-400 font-semibold"
            readOnly
          />
          <div className="text-xs text-slate-400">
            Suggested Intranet portals:
          </div>
          <div className="flex justify-center gap-3">
            <button onClick={() => navigateTo('vidyaos.local')} className="text-cyan-400 hover:underline">vidyaos.local</button>
            <button onClick={() => navigateTo('news.local')} className="text-cyan-400 hover:underline">news.local</button>
            <button onClick={() => navigateTo('mail.local')} className="text-cyan-400 hover:underline">mail.local</button>
          </div>
        </div>
        <div className="text-xs text-slate-500">Error 404: Host unresolved for other domains.</div>
      </div>
    );
  };

  return (
    <div className="flex-1 flex flex-col text-slate-200 text-xs h-full bg-slate-900">
      {/* Navigation bar */}
      <div className="flex items-center gap-2 p-2 border-b border-white/5 bg-slate-950/40">
        <button 
          onClick={handleBack} 
          disabled={historyIdx === 0} 
          className="p-1 rounded hover:bg-white/5 disabled:opacity-40 disabled:hover:bg-transparent"
        >
          <ChevronLeft size={16} />
        </button>
        <button 
          onClick={handleForward} 
          disabled={historyIdx === history.length - 1} 
          className="p-1 rounded hover:bg-white/5 disabled:opacity-40 disabled:hover:bg-transparent"
        >
          <ChevronRight size={16} />
        </button>
        
        <input 
          type="text" 
          value={navInput}
          onChange={(e) => setNavInput(e.target.value)}
          onKeyDown={(e) => e.key === 'Enter' && navigateTo(navInput)}
          className="flex-1 bg-black/45 border border-white/10 rounded px-3 py-1 font-mono text-cyan-400 text-xs focus:ring-0"
        />
      </div>
      
      {/* Viewport page render */}
      <div className="flex-1 overflow-y-auto bg-slate-950/60">
        {renderWebpageContent()}
      </div>
    </div>
  );
}

// 5. Settings App
interface SettingsProps {
  theme: 'dark' | 'light' | 'neon';
  setTheme: (t: 'dark' | 'light' | 'neon') => void;
  volume: number;
  setVolume: (v: number) => void;
  muted: boolean;
  setMuted: (m: boolean) => void;
  wifiEnabled: boolean;
  setWifiEnabled: (e: boolean) => void;
  vpnEnabled: boolean;
  setVpnEnabled: (e: boolean) => void;
  ip: string;
  setPasscode: (p: string) => void;
  currentUser: string;
}

function SettingsApp({ 
  theme, setTheme, volume, setVolume, muted, setMuted, 
  wifiEnabled, setWifiEnabled, vpnEnabled, setVpnEnabled, ip, setPasscode, currentUser 
}: SettingsProps) {
  const [activeTab, setActiveTab] = useState(0);
  const [passInput, setPassInput] = useState('');

  const tabs = ['Display', 'Appearance', 'Network', 'Accounts', 'Sound'];

  const handleUpdatePasscode = () => {
    if (currentUser !== 'root') {
      alert('Permission Denied: root credentials required to change passcode.');
      return;
    }
    if (passInput.trim().length >= 4) {
      setPasscode(passInput);
      alert('Security Passcode updated successfully.');
      setPassInput('');
    } else {
      alert('Passcode must be at least 4 characters.');
    }
  };

  return (
    <div className="flex-1 flex overflow-hidden text-slate-300 font-sans text-xs h-full">
      {/* Settings Navigation Sidebar */}
      <div className="w-[130px] border-r border-white/5 bg-black/20 flex flex-col p-2 gap-1 overflow-y-auto">
        <span className="text-[10px] text-slate-500 font-bold tracking-wider mb-2 uppercase">Control</span>
        {tabs.map((tab, idx) => (
          <button 
            key={tab}
            onClick={() => setActiveTab(idx)}
            className={`w-full text-left px-2 py-1.5 rounded transition-all ${activeTab === idx ? 'bg-cyan-500/20 text-cyan-400 font-semibold' : 'hover:bg-white/5'}`}
          >
            {tab}
          </button>
        ))}
      </div>
      
      {/* Settings content pane */}
      <div className="flex-1 p-4 overflow-y-auto flex flex-col gap-4">
        {activeTab === 0 && (
          <div className="flex flex-col gap-4">
            <h2 className="text-sm font-bold text-slate-200 border-b border-white/5 pb-2">Display settings</h2>
            <div className="flex flex-col gap-1">
              <span className="text-slate-400">Scale resolution: 1280x960 (4x)</span>
              <span className="text-[10px] text-slate-500">Auto-scaled dynamically by client container viewport size.</span>
            </div>
          </div>
        )}
        
        {activeTab === 1 && (
          <div className="flex flex-col gap-4">
            <h2 className="text-sm font-bold text-slate-200 border-b border-white/5 pb-2">Desktop Appearance</h2>
            <div className="flex flex-col gap-2">
              <span className="text-slate-400">Select active Skin:</span>
              <div className="flex gap-2 mt-1">
                {(['dark', 'light', 'neon'] as const).map(t => (
                  <button 
                    key={t}
                    onClick={() => setTheme(t)}
                    className={`px-3 py-1.5 rounded border capitalize ${theme === t ? 'bg-cyan-500/20 text-cyan-400 border-cyan-500/40' : 'bg-white/5 border-white/5 hover:bg-white/10'}`}
                  >
                    {t}
                  </button>
                ))}
              </div>
            </div>
          </div>
        )}
        
        {activeTab === 2 && (
          <div className="flex flex-col gap-4">
            <h2 className="text-sm font-bold text-slate-200 border-b border-white/5 pb-2">Network settings</h2>
            <div className="flex flex-col gap-3">
              <div className="flex justify-between items-center">
                <span className="text-slate-400">Wi-Fi Connection</span>
                <input 
                  type="checkbox" 
                  checked={wifiEnabled}
                  onChange={(e) => setWifiEnabled(e.target.checked)}
                  className="rounded bg-black/45 border-white/10 text-cyan-500 focus:ring-0"
                />
              </div>
              <div className="flex justify-between items-center">
                <span className="text-slate-400">Secure VPN Tunnel</span>
                <input 
                  type="checkbox" 
                  checked={vpnEnabled}
                  onChange={(e) => setVpnEnabled(e.target.checked)}
                  className="rounded bg-black/45 border-white/10 text-cyan-500 focus:ring-0"
                />
              </div>
              <div className="text-[11px] text-slate-500 border-t border-white/5 pt-2 mt-1 flex flex-col gap-1">
                <span>Assigned IP Address: <span className="font-mono text-cyan-400">{wifiEnabled ? ip : '0.0.0.0'}</span></span>
                <span>Gateway Node: <span className="font-mono text-cyan-400">192.168.1.1</span></span>
              </div>
            </div>
          </div>
        )}

        {activeTab === 3 && (
          <div className="flex flex-col gap-4">
            <h2 className="text-sm font-bold text-slate-200 border-b border-white/5 pb-2">Security Credentials</h2>
            <div className="flex flex-col gap-2">
              <span className="text-slate-400">Reset System Screen Passcode (current: root only)</span>
              <div className="flex gap-2 mt-2">
                <input 
                  type="password" 
                  value={passInput}
                  onChange={(e) => setPassInput(e.target.value)}
                  placeholder="New passcode..."
                  className="bg-black/45 border border-white/10 rounded px-2 py-1 text-xs text-cyan-400"
                />
                <button 
                  onClick={handleUpdatePasscode}
                  className="bg-cyan-500/20 hover:bg-cyan-500/30 text-cyan-400 border border-cyan-500/30 px-3 py-1 rounded"
                >
                  Save
                </button>
              </div>
            </div>
          </div>
        )}

        {activeTab === 4 && (
          <div className="flex flex-col gap-4">
            <h2 className="text-sm font-bold text-slate-200 border-b border-white/5 pb-2">Audio Sound Console</h2>
            <div className="flex flex-col gap-3">
              <div className="flex items-center justify-between">
                <span className="text-slate-400">Volume level: {volume}%</span>
                <input 
                  type="range" 
                  min="0" 
                  max="100" 
                  value={volume}
                  onChange={(e) => setVolume(parseInt(e.target.value))}
                  className="w-2/3 accent-cyan-400 bg-white/10 h-1.5 rounded-lg appearance-none cursor-pointer"
                />
              </div>
              <div className="flex justify-between items-center">
                <span className="text-slate-400">Mute Audio</span>
                <input 
                  type="checkbox" 
                  checked={muted}
                  onChange={(e) => setMuted(e.target.checked)}
                  className="rounded bg-black/45 border-white/10 text-cyan-500 focus:ring-0"
                />
              </div>
            </div>
          </div>
        )}
      </div>
    </div>
  );
}

// 6. Control Panel App (Firewall)
interface ControlPanelProps {
  rules: Record<string, boolean>;
  setRules: React.Dispatch<React.SetStateAction<Record<string, boolean>>>;
}

function ControlPanelApp({ rules, setRules }: ControlPanelProps) {
  const toggleRule = (app: string) => {
    setRules(prev => ({ ...prev, [app]: !prev[app] }));
  };

  return (
    <div className="flex-1 p-4 text-slate-300 font-sans text-xs h-full flex flex-col gap-4">
      <h2 className="text-sm font-bold text-slate-200 border-b border-white/5 pb-2">Firewall rules dashboard</h2>
      <p className="text-[11px] text-slate-500">Toggle networking permissions for individual simulated background services.</p>
      
      <div className="flex flex-col gap-3 mt-2">
        {Object.keys(rules).map(app => (
          <div key={app} className="glass p-3 rounded-lg border-white/5 flex items-center justify-between">
            <span className="font-semibold text-slate-200">{app} Connection Port</span>
            <div className="flex items-center gap-3">
              <span className={`text-[10px] font-bold uppercase tracking-wider ${rules[app] ? 'text-emerald-400' : 'text-red-400'}`}>
                {rules[app] ? 'Allowed' : 'Blocked'}
              </span>
              <button 
                onClick={() => toggleRule(app)}
                className={`px-3 py-1 rounded text-[11px] font-semibold border ${rules[app] ? 'bg-red-500/20 text-red-400 border-red-500/30' : 'bg-emerald-500/20 text-emerald-400 border-emerald-500/30'}`}
              >
                {rules[app] ? 'Block' : 'Allow'}
              </button>
            </div>
          </div>
        ))}
      </div>
    </div>
  );
}

// 7. Text Editor App
interface TextEditorProps {
  files: Record<string, string>;
  setFiles: React.Dispatch<React.SetStateAction<Record<string, string>>>;
  activeFile: string;
  setActiveFile: (file: string) => void;
}

function TextEditorApp({ files, setFiles, activeFile }: TextEditorProps) {
  const [content, setContent] = useState('');

  useEffect(() => {
    setContent(files[activeFile] || '');
  }, [activeFile, files]);

  const handleSave = () => {
    setFiles(prev => ({ ...prev, [activeFile]: content }));
    alert(`Successfully saved: ${activeFile}`);
  };

  return (
    <div className="flex-1 flex flex-col text-slate-200 text-xs h-full bg-slate-900">
      {/* File status ribbon */}
      <div className="flex justify-between items-center p-2 border-b border-white/5 bg-slate-950/40">
        <span className="text-slate-400">Editing: <span className="font-mono text-cyan-400">{activeFile}</span></span>
        <button 
          onClick={handleSave}
          className="bg-emerald-500/20 hover:bg-emerald-500/30 text-emerald-400 border border-emerald-500/30 px-3 py-1 rounded text-[11px]"
        >
          Save file
        </button>
      </div>
      
      {/* Editor textarea */}
      <textarea 
        value={content}
        onChange={(e) => setContent(e.target.value)}
        className="flex-1 bg-transparent border-none text-slate-200 font-mono text-xs focus:ring-0 p-4 resize-none select-text"
        placeholder="Start writing..."
      />
    </div>
  );
}

// 8. App Store App
interface AppStoreProps {
  installed: Record<string, boolean>;
  setInstalled: React.Dispatch<React.SetStateAction<Record<string, boolean>>>;
  addNotif: (msg: string) => void;
}

interface StoreItem {
  id: string;
  name: string;
  cat: string;
  desc: string;
  rating: number;
}

function AppStoreApp({ installed, setInstalled, addNotif }: AppStoreProps) {
  const [activeCategory, setActiveCategory] = useState<string>('All');
  const [installingId, setInstallingId] = useState<string | null>(null);
  const [progress, setProgress] = useState(0);

  const categories = ['All', 'Dev', 'Utils', 'Console'];

  const storeItems: StoreItem[] = [
    { id: 'cmatrix', name: 'Matrix Animation', cat: 'Console', desc: 'Simulated visual binary rain waterfall cascade.', rating: 5 },
    { id: 'python', name: 'Python Shell Runtime', cat: 'Dev', desc: 'Adds local scripting and interactive logic.', rating: 4 },
    { id: 'nodejs', name: 'Node.js Engine', cat: 'Dev', desc: 'Backend server JavaScript environment execution.', rating: 5 },
    { id: 'java', name: 'Java Virtual Compiler', cat: 'Dev', desc: 'Simulates compile options for Java source files.', rating: 4 },
    { id: 'gcc', name: 'GCC compiler suite', cat: 'Dev', desc: 'Natively compile virtual mock C/C++ files.', rating: 5 }
  ];

  const handleInstall = (id: string) => {
    if (installingId !== null) return;
    setInstallingId(id);
    setProgress(0);
  };

  useEffect(() => {
    if (installingId === null) return;
    const interval = setInterval(() => {
      setProgress(prev => {
        if (prev >= 100) {
          clearInterval(interval);
          setInstalled(old => ({ ...old, [installingId]: true }));
          addNotif(`Installed package: ${installingId}`);
          setInstallingId(null);
          return 0;
        }
        return prev + 25;
      });
    }, 1000);
    return () => clearInterval(interval);
  }, [installingId]);

  const getFilteredItems = () => {
    if (activeCategory === 'All') return storeItems;
    return storeItems.filter(item => item.cat === activeCategory);
  };

  return (
    <div className="flex-1 flex overflow-hidden text-slate-300 font-sans text-xs h-full">
      {/* Categories Sidebar */}
      <div className="w-[120px] border-r border-white/5 bg-black/20 flex flex-col p-2 gap-1 overflow-y-auto">
        <span className="text-[10px] text-slate-500 font-bold tracking-wider mb-2 uppercase">Categories</span>
        {categories.map(cat => (
          <button 
            key={cat}
            onClick={() => setActiveCategory(cat)}
            className={`w-full text-left px-2 py-1.5 rounded transition-all ${activeCategory === cat ? 'bg-cyan-500/20 text-cyan-400 font-semibold' : 'hover:bg-white/5'}`}
          >
            {cat}
          </button>
        ))}
      </div>
      
      {/* App list cards */}
      <div className="flex-1 p-4 overflow-y-auto flex flex-col gap-3">
        {getFilteredItems().map(item => {
          const isInstalled = installed[item.id];
          const isInstalling = installingId === item.id;
          
          return (
            <div key={item.id} className="glass p-3 rounded-lg border-white/5 flex items-center justify-between">
              <div className="flex flex-col gap-1 max-w-[70%]">
                <div className="flex items-center gap-2">
                  <span className="font-bold text-slate-200">{item.name}</span>
                  <span className="bg-cyan-500/10 text-cyan-400 text-[9px] px-1.5 py-0.5 rounded font-mono uppercase">{item.cat}</span>
                </div>
                <span className="text-[10px] text-slate-500 leading-relaxed">{item.desc}</span>
              </div>
              
              <div className="shrink-0 flex flex-col items-end gap-1.5">
                {isInstalled ? (
                  <span className="bg-emerald-500/20 text-emerald-400 border border-emerald-500/30 px-3 py-1.5 rounded font-semibold text-[10px]">
                    Installed
                  </span>
                ) : isInstalling ? (
                  <div className="flex flex-col items-center gap-1">
                    <span className="text-[10px] text-cyan-400 font-mono font-bold">{progress}%</span>
                    <div className="w-16 h-1.5 bg-white/5 rounded-full overflow-hidden">
                      <div className="h-full bg-cyan-400 transition-all duration-300" style={{ width: `${progress}%` }} />
                    </div>
                  </div>
                ) : (
                  <button 
                    onClick={() => handleInstall(item.id)}
                    disabled={installingId !== null}
                    className="bg-cyan-500/20 hover:bg-cyan-500/30 text-cyan-400 border border-cyan-500/30 px-3 py-1.5 rounded font-semibold text-[10px]"
                  >
                    Install
                  </button>
                )}
              </div>
            </div>
          );
        })}
      </div>
    </div>
  );
}
