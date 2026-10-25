let traceData = null;
let viewMode = 'perf'; // 'perf', 'correlation', or 'solver'
let currentStep = 0;
let maxStep = 0;

// Solver mode caches
let stateAtStep = [];
let queueAtStep = [];
let cfgZoom = null;
let hraZoom = null;

// Correlated mode state
let currentCorrStep = 0;
let maxCorrStep = 0;
let selectedConfig = 'C'; // 'A', 'B', or 'C'
let corrHraZoom = null;

// Phase 15: Performance Dashboard state
let perfData = null;    // parsed performance_data.json
let selectedBm = 'BM03'; // default benchmark to display

// File loader
document.getElementById('traceFile').addEventListener('change', function(e) {
    const file = e.target.files[0];
    if (!file) return;
    
    const reader = new FileReader();
    reader.onload = function(e) {
        try {
            traceData = JSON.parse(e.target.result);
            mermaid.initialize({ startOnLoad: false, theme: 'dark', maxTextSize: 90000 });
            
            // Check if this is a Phase 14 Correlated Trace or Phase 13 Solver Trace
            if (traceData.correlated_events) {
                // Phase 14 Correlated Trace
                document.getElementById('loadStatus').innerText = "Loaded Phase 14 Correlated Trace: " + (traceData.benchmark || file.name);
                document.getElementById('viewTabs').style.display = 'flex';
                document.getElementById('traceControls').style.display = 'flex';
                document.getElementById('pipeline-indicator').style.display = 'flex';
                document.getElementById('currentEventPanel').style.display = 'block';
                
                switchViewMode('correlation');
                initCorrelationVisualization();
            } else if (traceData.trace) {
                // Phase 13 Solver Trace
                document.getElementById('loadStatus').innerText = "Loaded Phase 13 Solver Trace: Config " + (traceData.config || "A");
                document.getElementById('viewTabs').style.display = 'flex';
                document.getElementById('traceControls').style.display = 'flex';
                document.getElementById('pipeline-indicator').style.display = 'flex';
                document.getElementById('currentEventPanel').style.display = 'block';
                
                switchViewMode('solver');
                initVisualization();
            } else {
                throw new Error("Unrecognized trace format (neither correlated_events nor trace array found).");
            }
        } catch (err) {
            document.getElementById('loadStatus').innerText = "Error parsing JSON: " + err.message;
            document.getElementById('loadStatus').style.color = "red";
        }
    };
    reader.readAsText(file);
});

function switchViewMode(mode) {
    viewMode = mode;
    // Update tab button active states
    document.getElementById('tabPerf').classList.toggle('active', mode === 'perf');
    document.getElementById('tabCorr').classList.toggle('active', mode === 'correlation');
    document.getElementById('tabSolver').classList.toggle('active', mode === 'solver');

    document.getElementById('perfPanels').style.display        = (mode === 'perf')        ? 'flex' : 'none';
    document.getElementById('correlationPanels').style.display = (mode === 'correlation') ? 'flex' : 'none';
    document.getElementById('mainPanels').style.display        = (mode === 'solver')      ? 'flex' : 'none';

    if (mode === 'perf') {
        if (!perfData) loadPerfData();
        else renderPerfDashboard();
    } else if (mode === 'correlation' && traceData && traceData.correlated_events) {
        updateCorrPanels();
    } else if (mode === 'solver' && traceData && traceData.trace) {
        updateAllPanels();
    }
}

// ============================================================
// PHASE 14: CORRELATION VISUALIZATION ENGINE
// ============================================================

function initCorrelationVisualization() {
    maxCorrStep = traceData.correlated_events.length;
    currentCorrStep = 1; // 1-based step for correlated events (1 to N)

    renderCorrStats();
    renderCorrTimeline();
    updateCorrPanels();
}

function selectStaticConfig(cfg) {
    selectedConfig = cfg;
    document.getElementById('cfgBtnA').classList.toggle('btn-active', cfg === 'A');
    document.getElementById('cfgBtnB').classList.toggle('btn-active', cfg === 'B');
    document.getElementById('cfgBtnC').classList.toggle('btn-active', cfg === 'C');
    updateCorrPanels();
}

function renderCorrStats() {
    const rs = traceData.runtime_stats || {};
    const ss = traceData.static_stats || {};
    
    let html = `
        <tr><th>Metric Category</th><th>Concrete Runtime</th><th>Static Analysis (${selectedConfig})</th></tr>
        <tr><td>Domain Nature</td><td><span class="badge badge-concrete">CONCRETE RUNTIME</span> (1 Executed Path)</td><td><span class="badge badge-static">STATIC INVARIANT</span> (All Sound Paths)</td></tr>
        <tr><td>Loop Iterations</td><td><b>${rs.loop_iterations || 0}</b> (Real CPU loop trips)</td><td><b>${ss['worklist_iters_' + selectedConfig] || 0}</b> (CFG Worklist fixpoint trips)</td></tr>
        <tr><td>Allocations / Malloc</td><td><b>${rs.malloc_count || 0}</b> concrete mallocs</td><td><b>${ss.transfers_A || ss['transfers_' + selectedConfig] || 0}</b> transfer functions</td></tr>
        <tr><td>Branch Evaluations</td><td><b>${rs.branch_count || 0}</b> dynamic branch outcomes</td><td><b>${ss['pruned_' + selectedConfig] || 0}</b> statements pruned (B/C)</td></tr>
        <tr><td>MDE Reuse (Hits / Misses)</td><td>N/A (physical addresses)</td><td><b>${ss.mde_hits || 0}</b> hits / <b>${ss.mde_misses || 0}</b> misses</td></tr>
        <tr><td>Alloc Site Mappings</td><td colspan="2"><b>${(traceData.alloc_site_map || []).length}</b> mapped sites (Visualization Bridge)</td></tr>
    `;
    document.getElementById('corrStatsTable').innerHTML = html;
}

function renderCorrTimeline() {
    const el = document.getElementById('corrTimeline');
    el.innerHTML = '';
    
    (traceData.correlated_events || []).forEach((ce, idx) => {
        const stepNum = idx + 1;
        let cType = ce.concrete ? ce.concrete.type : "N/A";
        let cAddr = (ce.concrete && ce.concrete.addr) ? ce.concrete.addr : "";
        let op = ce.operation || "op";
        
        el.innerHTML += `
            <div class="log-entry" id="corr-log-${stepNum}" onclick="goToCorrStep(${stepNum})">
                [Step ${stepNum}] Line ${ce.line}: <span style="color:#4ec9b0;font-weight:bold;">${cType}</span> (${op}) ${cAddr ? `addr=${cAddr}` : ''}
            </div>
        `;
    });
}

function goToCorrStep(step) {
    if (step < 1) step = 1;
    if (step > maxCorrStep) step = maxCorrStep;
    currentCorrStep = step;
    updateCorrPanels();
}

function updateCorrPanels() {
    if (!traceData || !traceData.correlated_events || traceData.correlated_events.length === 0) return;
    
    // Highlight active timeline entry
    document.querySelectorAll('#corrTimeline .log-entry').forEach(e => e.classList.remove('active'));
    const activeEntry = document.getElementById(`corr-log-${currentCorrStep}`);
    if (activeEntry) {
        activeEntry.classList.add('active');
        activeEntry.scrollIntoView({ block: 'nearest' });
    }

    const ce = traceData.correlated_events[currentCorrStep - 1];
    document.getElementById('stepCounter').innerText = `Step ${currentCorrStep} / ${maxCorrStep} [Line ${ce.line}]`;

    // 1. Update Current Event Inspector
    updateCorrEventInspector(ce);

    // 2. Update Concrete State View
    updateConcreteStateView(currentCorrStep);

    // 3. Update Static Abstract Heap Graph
    updateStaticAbstractGraph(ce);

    // 4. Update Synchronized Source View
    updateCorrSourceView(ce);

    // 5. Update Correlation Bridge View
    updateCorrBridgeView(ce);

    // 6. Update Pipeline Stages
    document.getElementById('stage-concrete').classList.add('active');
    document.getElementById('stage-cfg').classList.toggle('active', ce.operation !== 'unknown');
    document.getElementById('stage-liveness').classList.toggle('active', selectedConfig !== 'A');
    document.getElementById('stage-hra').classList.add('active');
    document.getElementById('stage-mde').classList.toggle('active', selectedConfig === 'C');
    document.getElementById('stage-fixpoint').classList.toggle('active', currentCorrStep === maxCorrStep);
}

function updateCorrEventInspector(ce) {
    const sev = ce['static_' + selectedConfig];
    
    let out = `
        <div class="kv-pair"><span class="kv-key">Source Line & Operation:</span><span class="kv-val" style="color:#569cd6;font-size:1.1em;font-weight:bold;">Line ${ce.line} — ${ce.operation.toUpperCase()}${ce.occurrence_index > 1 ? ` (Occ #${ce.occurrence_index})` : ''}</span></div>
        ${ce.correlation_status ? `<div class="kv-pair"><span class="kv-key">Correlation Status:</span><span class="kv-val" style="color:${ce.correlation_status === 'MATCHED' ? '#4ec9b0' : (ce.correlation_status === 'NOT_APPLICABLE_TO_STATIC_ANALYSIS' ? '#888' : '#e5c07b')};font-weight:bold;">${ce.correlation_status}</span></div>` : ''}
        <hr style="border-color:#333;margin:6px 0;">
        <div style="color:#4ec9b0;font-weight:bold;margin-bottom:4px;">CONCRETE RUNTIME:</div>
    `;
    
    if (ce.concrete) {
        const c = ce.concrete;
        out += `
            <div class="kv-pair"><span class="kv-key">Event Type:</span><span class="kv-val">${c.type}</span></div>
            ${c.var ? `<div class="kv-pair"><span class="kv-key">Variable:</span><span class="kv-val">${c.var}</span></div>` : ''}
            ${c.addr && c.addr !== '0x0' ? `<div class="kv-pair"><span class="kv-key">Concrete Address:</span><span class="kv-val" style="color:#4ec9b0;">${c.addr}</span></div>` : ''}
            ${c.field ? `<div class="kv-pair"><span class="kv-key">Field:</span><span class="kv-val">${c.field} &rarr; ${c.addr2 || '0x0'}</span></div>` : ''}
            ${c.alloc_site > 0 ? `<div class="kv-pair"><span class="kv-key">Alloc Site:</span><span class="kv-val">#${c.alloc_site}</span></div>` : ''}
            ${c.branch_taken !== -1 ? `<div class="kv-pair"><span class="kv-key">Branch Taken:</span><span class="kv-val">${c.branch_taken === 1 ? 'TRUE (Taken)' : 'FALSE (Not taken)'}</span></div>` : ''}
            ${c.loop_iter > 0 ? `<div class="kv-pair"><span class="kv-key">Loop Iteration:</span><span class="kv-val">Trip #${c.loop_iter}</span></div>` : ''}
        `;
    } else {
        out += `<div class="kv-val" style="color:#888;">No concrete event at this program point.</div>`;
    }

    out += `
        <hr style="border-color:#333;margin:6px 0;">
        <div style="color:#9cdcfe;font-weight:bold;margin-bottom:4px;">STATIC ANALYSIS (Config ${selectedConfig}):</div>
    `;

    if (sev) {
        let status = sev.pruned ? "<span class='badge badge-pruned'>PRUNED</span>" : "<span class='badge badge-live'>LIVE</span>";
        if (selectedConfig === 'C' && sev.mde_hit !== undefined) {
            status += sev.mde_hit ? " <span class='badge' style='background:#0e639c;'>MDE HIT</span>" : " <span class='badge' style='background:#6a9955;'>MDE NEW</span>";
        }
        out += `
            <div class="kv-pair"><span class="kv-key">IR Statement:</span><span class="kv-val">${escapeHtml(sev.stmt || '')}</span></div>
            <div class="kv-pair"><span class="kv-key">Transfer Status:</span><span class="kv-val">${status}</span></div>
        `;
    } else {
        out += `<div class="kv-val" style="color:#888;">No static event recorded for Config ${selectedConfig}.</div>`;
    }

    if (ce.alloc_correlation) {
        out += `
            <hr style="border-color:#333;margin:6px 0;">
            <div class="kv-pair"><span class="kv-key">Visualization Bridge:</span><span class="kv-val" style="color:#ce9178;">${ce.alloc_correlation.concrete_addr} &harr; alloc#${ce.alloc_correlation.alloc_site} &harr; ${ce.alloc_correlation.abstract_node}</span></div>
        `;
    }

    document.getElementById('ce-content').innerHTML = out;
}

function updateConcreteStateView(upToStep) {
    // Reconstruct concrete memory map up to current step
    const knownVars = {};
    const knownAllocations = {};
    const knownFields = [];
    let lastLoopIter = 0;
    let lastBranch = "None";

    for (let i = 0; i < upToStep; i++) {
        const ce = traceData.correlated_events[i];
        if (!ce.concrete) continue;
        const c = ce.concrete;

        if (c.type === 'RT_MALLOC' && c.addr && c.addr !== '0x0') {
            knownAllocations[c.addr] = { site: c.alloc_site, var: c.var };
            if (c.var) knownVars[c.var] = c.addr;
        } else if (c.type === 'RT_PTR_ASSIGN' && c.var && c.addr) {
            knownVars[c.var] = c.addr;
        } else if (c.type === 'RT_FIELD_STORE' && c.addr && c.field) {
            knownFields.push({ base: c.addr, field: c.field, target: c.addr2 });
        } else if (c.type === 'RT_BRANCH') {
            lastBranch = (c.branch_taken === 1) ? "Taken (1)" : "Not Taken (0)";
        } else if (c.type === 'RT_LOOP_ITER') {
            lastLoopIter = c.loop_iter;
        }
    }

    let html = `
        <div style="margin-bottom:12px;">
            <div class="kv-key">ACTIVE CONCRETE POINTER VARIABLES:</div>
            <table>
                <tr><th>Variable</th><th>Concrete Address (64-bit Hex)</th><th>Status</th></tr>
    `;
    const varKeys = Object.keys(knownVars);
    if (varKeys.length === 0) {
        html += `<tr><td colspan="3" style="color:#888;">No variables initialized yet.</td></tr>`;
    } else {
        varKeys.forEach(v => {
            html += `<tr><td><b>${v}</b></td><td style="color:#4ec9b0;font-family:Consolas;">${knownVars[v]}</td><td><span class="badge badge-concrete">VALID PTR</span></td></tr>`;
        });
    }
    html += `</table></div>`;

    html += `
        <div style="margin-bottom:12px;">
            <div class="kv-key">CONCRETE HEAP ALLOCATIONS:</div>
            <table>
                <tr><th>Memory Address</th><th>Alloc Site</th><th>Bound To</th></tr>
    `;
    const allocKeys = Object.keys(knownAllocations);
    if (allocKeys.length === 0) {
        html += `<tr><td colspan="3" style="color:#888;">No heap blocks allocated yet.</td></tr>`;
    } else {
        allocKeys.forEach(a => {
            html += `<tr><td style="color:#4ec9b0;font-family:Consolas;">${a}</td><td>alloc#${knownAllocations[a].site}</td><td>${knownAllocations[a].var || '-'}</td></tr>`;
        });
    }
    html += `</table></div>`;

    html += `
        <div style="display:flex; gap:15px;">
            <div style="flex:1;">
                <div class="kv-key">CURRENT LOOP COUNTER:</div>
                <div style="font-size:1.1em; color:#4ec9b0; font-weight:bold;">${lastLoopIter > 0 ? `Trip #${lastLoopIter}` : '0 (Non-loop)'}</div>
            </div>
            <div style="flex:1;">
                <div class="kv-key">LAST BRANCH OUTCOME:</div>
                <div style="font-size:1.1em; color:#9cdcfe; font-weight:bold;">${lastBranch}</div>
            </div>
        </div>
    `;

    document.getElementById('concreteStateView').innerHTML = html;
}

function updateStaticAbstractGraph(ce) {
    const sev = ce['static_' + selectedConfig];
    const targetDiv = document.getElementById('mermaid-corr-hra');

    if (!sev || !sev.state) {
        targetDiv.innerHTML = `<div style="padding:20px; color:#888; text-align:center;">No static state at this step for Config ${selectedConfig}.</div>`;
        return;
    }

    const state = sev.state;
    let m = "graph TD;\n";

    if (!state.nodes || state.nodes.length === 0) {
        m += "EMPTY((Empty Heap));\n";
    } else {
        state.nodes.forEach(n => {
            m += `N${n}["Node ${n} (alloc#${n})"];\n`;
        });
        if (state.edges) {
            state.edges.forEach(e => {
                m += `N${e.source} -- ${e.field} --> N${e.target};\n`;
            });
        }
        if (state.bindings) {
            for (const [varName, nList] of Object.entries(state.bindings)) {
                let cleanVar = varName.replace(/[^a-zA-Z0-9_]/g, '');
                m += `V_${cleanVar}["var ${escapeHtml(varName)}"]:::varStyle;\n`;
                (nList || []).forEach(nid => {
                    m += `V_${cleanVar} -.-> N${nid};\n`;
                });
            }
        }
    }
    m += "classDef varStyle fill:#0e639c,stroke:#569cd6,color:#fff;\n";

    targetDiv.removeAttribute('data-processed');
    mermaid.render('mermaid-corr-hra-svg', m).then(res => {
        targetDiv.innerHTML = res.svg;
        const svgEl = targetDiv.querySelector('svg');
        if (svgEl) {
            svgEl.style.width = '100%';
            svgEl.style.height = '100%';
            if (corrHraZoom) corrHraZoom.destroy();
            corrHraZoom = svgPanZoom(svgEl, { zoomEnabled: true, controlIconsEnabled: false, fit: true, center: true });
        }
    }).catch(err => {
        targetDiv.innerHTML = `<pre style="color:red;">Render Error: ${err.message}</pre>`;
    });
}

function updateCorrSourceView(currentCe) {
    let html = `<div style="font-family:Consolas,monospace; font-size:0.95em;">`;
    
    (traceData.correlated_events || []).forEach((ce, idx) => {
        const stepNum = idx + 1;
        const isActive = (stepNum === currentCorrStep);
        const bg = isActive ? "background:#094771; color:#fff; border-left:3px solid #007acc;" : "border-left:3px solid transparent;";
        const sev = ce['static_' + selectedConfig];
        const stmtText = (sev && sev.stmt) ? sev.stmt : ce.operation;

        html += `
            <div style="${bg} padding: 4px 8px; margin: 2px 0; border-radius: 2px; cursor: pointer;" onclick="goToCorrStep(${stepNum})">
                <span style="color:#858585; display:inline-block; width:30px;">${ce.line}:</span>
                <span style="font-weight:bold;">${escapeHtml(stmtText)}</span>
                ${ce.concrete ? `<span class="badge badge-concrete">${ce.concrete.type}</span>` : ''}
                ${sev && sev.pruned ? `<span class="badge badge-pruned">PRUNED</span>` : ''}
            </div>
        `;
    });
    
    html += `</div>`;
    document.getElementById('corrSourceView').innerHTML = html;
}

function updateCorrBridgeView(ce) {
    let html = `
        <div style="margin-bottom:12px;">
            <div class="kv-key">ALLOCATION SITE MAPPING (VISUALIZATION BRIDGE ONLY):</div>
            <p style="font-size:0.85em; color:#aaa; margin:4px 0 8px 0;">
                Connects physical memory addresses produced during execution to abstract analysis nodes.
                This mapping does <b>not</b> modify HRA or MDE semantics.
            </p>
            <table>
                <tr><th>Concrete Address</th><th>Alloc Site</th><th>Abstract Node</th><th>Domain Meaning</th></tr>
    `;
    const allocMap = traceData.alloc_site_map || [];
    if (allocMap.length === 0) {
        html += `<tr><td colspan="4" style="color:#888;">No allocation site correlations.</td></tr>`;
    } else {
        allocMap.forEach(ac => {
            const isMatch = (ce.concrete && ce.concrete.addr === ac.concrete_addr);
            const rowStyle = isMatch ? "background:rgba(206,145,120,0.2); font-weight:bold;" : "";
            html += `
                <tr style="${rowStyle}">
                    <td style="color:#4ec9b0;font-family:Consolas;">${ac.concrete_addr}</td>
                    <td>alloc#${ac.alloc_site}</td>
                    <td style="color:#9cdcfe;">${ac.abstract_node}</td>
                    <td>Concrete Heap Block &harr; Static Shape Equivalence Node</td>
                </tr>
            `;
        });
    }
    html += `</table></div>`;

    html += `
        <div style="margin-top:15px; border-top:1px solid #3c3c3c; padding-top:10px;">
            <div class="kv-key">SEMANTIC SEPARATION PRINCIPLE:</div>
            <ul style="font-size:0.85em; color:#ccc; margin:5px 0; padding-left:20px; line-height:1.5;">
                <li><b>Single Execution Path vs All Paths:</b> The runtime trace observes exactly 1 execution trace of benchmark ${traceData.benchmark}. Static analysis models all valid executions safely.</li>
                <li><b>Address Independence:</b> Concrete memory addresses vary across process runs. HRA nodes (${selectedConfig}) are strictly canonicalized and deterministic.</li>
                <li><b>Pruning Distinction:</b> Static Config B/C prunes unneeded abstract transfers via liveness. The concrete binary executed physical statements.</li>
            </ul>
        </div>
    `;

    document.getElementById('corrBridgeView').innerHTML = html;
}

// ============================================================
// PHASE 13: ORIGINAL SOLVER TRACE ENGINE (PRESERVED)
// ============================================================

function initVisualization() {
    maxStep = traceData.trace.length;
    
    let lastState = null;
    let lastQueue = [];
    stateAtStep = new Array(maxStep + 1).fill(null);
    queueAtStep = new Array(maxStep + 1).fill([]);
    
    for (let i = 0; i < maxStep; i++) {
        let ev = traceData.trace[i];
        if (ev.state) lastState = ev.state;
        if (ev.queue) lastQueue = ev.queue;
        stateAtStep[i+1] = lastState;
        queueAtStep[i+1] = lastQueue;
    }
    
    renderStats();
    renderIR();
    renderCFGGraph();
    
    const el = document.getElementById('eventLog');
    el.innerHTML = '';
    el.innerHTML += `<div class="log-entry" id="log-0" onclick="goToStep(0)">[INITIAL] No events executed.</div>`;
    for (let i = 0; i < maxStep; i++) {
        let ev = traceData.trace[i];
        let msg = `${ev.type} ${ev.stmt || ev.blockId || ''}`;
        el.innerHTML += `<div class="log-entry" id="log-${i+1}" onclick="goToStep(${i+1})">[Step ${i+1}] ${escapeHtml(msg.trim())}</div>`;
    }
    
    currentStep = 0;
    updateAllPanels();
}

function renderStats() {
    const s = traceData.stats;
    let statsHtml = `
        <tr><th>Configuration</th><td>${traceData.config}</td></tr>
        <tr><th>Transfers Executed</th><td>${s.transfers}</td></tr>
        <tr><th>Worklist Iterations</th><td>${s.worklistIterations}</td></tr>
        <tr><th>Statements Examined</th><td>${s.statementsExamined}</td></tr>
        <tr><th>Statements Pruned</th><td>${s.statementsPruned}</td></tr>
        <tr><th>Bindings Removed</th><td>${s.bindingsRemoved}</td></tr>
        <tr><th>States Constructed</th><td>${s.statesConstructed}</td></tr>
    `;
    if (traceData.config === 'C') {
        document.getElementById('mdeViewPanel').style.display = 'flex';
        let hr = (s.mdeHits + s.mdeMisses > 0) ? ((s.mdeHits/(s.mdeHits+s.mdeMisses))*100).toFixed(2) : 0;
        statsHtml += `
            <tr><th>MDE Hits</th><td>${s.mdeHits}</td></tr>
            <tr><th>MDE Misses</th><td>${s.mdeMisses}</td></tr>
            <tr><th>Hit Rate</th><td>${hr}%</td></tr>
        `;
    } else {
        document.getElementById('mdeViewPanel').style.display = 'none';
    }
    document.getElementById('statsTable').innerHTML = statsHtml;
}

function renderIR() {
    let cfgHtml = '';
    for (const [blockId, block] of Object.entries(traceData.cfg)) {
        cfgHtml += `<div class="cfg-node" id="cfg-block-${blockId}">`;
        cfgHtml += `<div style="color:#569cd6;font-weight:bold;margin-bottom:4px;">Block ${blockId} (Pred: ${block.predecessors.join(',')}, Succ: ${block.successors.join(',')})</div>`;
        block.statements.forEach((stmt, idx) => {
            cfgHtml += `<div class="cfg-stmt" id="cfg-stmt-${blockId}-${idx}" data-stmt="${escapeHtml(stmt.text)}">`;
            cfgHtml += `<span>${escapeHtml(stmt.text)}</span>`;
            if (traceData.liveness && traceData.liveness[stmt.text]) {
                const live = traceData.liveness[stmt.text];
                cfgHtml += `<span style="font-size:0.8em; color:#858585; margin-left:10px;">[Use: ${live.use.join(',') || 'none'} | Def: ${live.def.join(',') || 'none'}]</span>`;
            }
            cfgHtml += `</div>`;
        });
        cfgHtml += `</div>`;
    }
    document.getElementById('cfgView').innerHTML = cfgHtml;
}

function renderCFGGraph() {
    let m = "graph TD;\n";
    for (const [blockId, block] of Object.entries(traceData.cfg)) {
        let label = `Block ${blockId}`;
        m += `B${blockId}["${label}"];\n`;
        block.successors.forEach(succ => {
            if (succ <= blockId && succ !== 0) {
                m += `B${blockId} -- BACK-EDGE --> B${succ};\n`;
            } else {
                m += `B${blockId} --> B${succ};\n`;
            }
        });
    }
    
    const targetDiv = document.getElementById('mermaid-cfg');
    targetDiv.removeAttribute('data-processed');
    mermaid.render('mermaid-cfg-svg', m).then(res => {
        targetDiv.innerHTML = res.svg;
        const svgEl = targetDiv.querySelector('svg');
        if (svgEl) {
            svgEl.style.width = '100%';
            svgEl.style.height = '100%';
            if (cfgZoom) cfgZoom.destroy();
            cfgZoom = svgPanZoom(svgEl, { zoomEnabled: true, controlIconsEnabled: false, fit: true, center: true });
        }
    }).catch(err => {
        targetDiv.innerHTML = `<pre style="color:red;">Render Error: ${err.message}</pre>`;
    });
}

function renderHRAGraph(state) {
    const targetDiv = document.getElementById('mermaid-hra');
    if (!state || (!state.nodes || state.nodes.length === 0)) {
        targetDiv.innerHTML = `<div style="padding:20px; color:#888; text-align:center;">Empty Heap State.</div>`;
        return;
    }
    
    let m = "graph TD;\n";
    state.nodes.forEach(n => {
        m += `N${n}["Node ${n}"];\n`;
    });
    if (state.edges) {
        state.edges.forEach(e => {
            m += `N${e.source} -- ${e.field} --> N${e.target};\n`;
        });
    }
    if (state.bindings) {
        for (const [varName, nList] of Object.entries(state.bindings)) {
            let cleanVar = varName.replace(/[^a-zA-Z0-9_]/g, '');
            m += `V_${cleanVar}["var ${escapeHtml(varName)}"]:::varStyle;\n`;
            (nList || []).forEach(nid => {
                m += `V_${cleanVar} -.-> N${nid};\n`;
            });
        }
    }
    m += "classDef varStyle fill:#0e639c,stroke:#569cd6,color:#fff;\n";

    targetDiv.removeAttribute('data-processed');
    mermaid.render('mermaid-hra-svg', m).then(res => {
        targetDiv.innerHTML = res.svg;
        const svgEl = targetDiv.querySelector('svg');
        if (svgEl) {
            svgEl.style.width = '100%';
            svgEl.style.height = '100%';
            if (hraZoom) hraZoom.destroy();
            hraZoom = svgPanZoom(svgEl, { zoomEnabled: true, controlIconsEnabled: false, fit: true, center: true });
        }
    }).catch(err => {
        targetDiv.innerHTML = `<pre style="color:red;">Render Error: ${err.message}</pre>`;
    });
}

function goToStep(step) {
    if (step < 0) step = 0;
    if (step > maxStep) step = maxStep;
    currentStep = step;
    updateAllPanels();
}

function updateAllPanels() {
    document.querySelectorAll('#eventLog .log-entry').forEach(e => e.classList.remove('active'));
    const activeEntry = document.getElementById(`log-${currentStep}`);
    if (activeEntry) {
        activeEntry.classList.add('active');
        activeEntry.scrollIntoView({ block: 'nearest' });
    }
    
    document.getElementById('stepCounter').innerText = `Step: ${currentStep} / ${maxStep}`;
    
    let ev = currentStep > 0 ? traceData.trace[currentStep - 1] : { type: 'INITIAL', desc: 'Solver ready.' };
    let state = stateAtStep[currentStep];
    let oldState = currentStep > 0 ? stateAtStep[currentStep - 1] : null;
    let queue = queueAtStep[currentStep];
    
    updateCurrentEventInspector(ev, oldState, state, null);
    renderHRAGraph(state);
    
    document.getElementById('graphView').innerHTML = formatState(state);
    let diff = computeDiff(oldState, state);
    document.getElementById('stateDiffView').innerHTML = diff;
    
    updateCFG(ev);
    updateWorklist(queue, ev);
    updateMDE(ev);
    updateCausality(ev, null);
}

function updateCFG(ev) {
    document.querySelectorAll('.cfg-node').forEach(n => n.classList.remove('active'));
    document.querySelectorAll('.cfg-stmt').forEach(s => s.classList.remove('active'));
    
    if (ev.blockId !== undefined) {
        const el = document.getElementById(`cfg-block-${ev.blockId}`);
        if (el) el.classList.add('active');
    }
    if (ev.stmt) {
        document.querySelectorAll('.cfg-stmt').forEach(s => {
            if (s.getAttribute('data-stmt') === ev.stmt) {
                s.classList.add('active');
            }
        });
    }
}

function updateWorklist(queue, ev) {
    let out = `<b>Current Queue:</b> [${queue.join(', ')}]\n\n`;
    if (ev.type === 'BlockPop') {
        out += `<span style="color:#4ec9b0;">Action: POP Block ${ev.blockId}</span>`;
    } else if (ev.type === 'WorklistPush') {
        out += `<span style="color:#dcdcaa;">Action: PUSH Block ${ev.blockId}</span>`;
    } else {
        out += `Action: Processing Block ${ev.blockId !== undefined ? ev.blockId : '-'}`;
    }
    document.getElementById('worklistView').innerHTML = `<pre>${out}</pre>`;
}

function updateMDE(ev) {
    if (ev.type === 'Canonicalize') {
        let out = `MDE LOOKUP\n    &darr;\n`;
        if (ev.isNew) {
            out += `MISS\n    &darr;\nAction: Create new canonical representation\n`;
        } else {
            out += `HIT\n    &darr;\nExisting index: #${ev.mdeIndex}\n    &darr;\nAction: Reuse canonical representation\n`;
        }
        document.getElementById('mdeLog').innerHTML = `<pre>${out}</pre>`;
    } else {
        document.getElementById('mdeLog').innerHTML = "No MDE canonicalization occurred at this trace position.";
    }
}

function updateCurrentEventInspector(ev, oldState, state, livenessInfo) {
    let out = `
        <div class="kv-pair"><span class="kv-key">Step:</span><span class="kv-val">${currentStep}</span></div>
        <div class="kv-pair"><span class="kv-key">Event type:</span><span class="kv-val">${ev.type}</span></div>
    `;
    if (ev.blockId !== undefined) out += `<div class="kv-pair"><span class="kv-key">CFG Block:</span><span class="kv-val">${ev.blockId}</span></div>`;
    if (ev.stmt) out += `<div class="kv-pair"><span class="kv-key">Statement:</span><span class="kv-val">${escapeHtml(ev.stmt)}</span></div>`;
    
    let status = "N/A";
    if (ev.type === 'StmtPost') status = ev.pruned ? "PRUNED" : "LIVE";
    if (ev.type === 'Canonicalize') status = ev.isNew ? "MISS" : "HIT";
    
    out += `<div class="kv-pair"><span class="kv-key">Status:</span><span class="kv-val">${status}</span></div>`;
    out += `<hr style="border-color:#333;">`;
    out += `<div class="kv-pair"><span class="kv-key">Before:</span><pre class="kv-val" style="margin:0;">${formatStateBrief(oldState)}</pre></div>`;
    out += `<div class="kv-pair"><span class="kv-key">After:</span><pre class="kv-val" style="margin:0;">${formatStateBrief(state)}</pre></div>`;
    
    let diff = computeDiff(oldState, state);
    if (!ev.state) diff = "No state transition at this event.";
    out += `<div class="kv-pair"><span class="kv-key">Change:</span><pre class="kv-val" style="margin:0;">${diff}</pre></div>`;
    
    document.getElementById('ce-content').innerHTML = out;
}

function updateCausality(ev, livenessInfo) {
    let out = "";
    if (ev.type === 'StmtPre') {
        out = `<b>Why?</b>\nThe statement is about to be evaluated. HRA state is prepared.\n`;
    } else if (ev.type === 'StmtPost') {
        if (ev.pruned) {
            out = `<b>Why?</b>\nThe statement was PRUNED because its destination variable is not live after this statement. The HRA state is unchanged.\n`;
        } else {
            let stmt = ev.stmt || "";
            if (stmt.includes("alloc")) {
                out = `<b>Why?</b>\nThis statement creates an abstract heap node for the allocation site.\n`;
            } else if (stmt.includes("->") && stmt.indexOf("=") > stmt.indexOf("->")) {
                out = `<b>Why?</b>\nThe load statement follows the selected field edge in the HRA state.\n`;
            } else if (stmt.includes("->") && stmt.indexOf("=") < stmt.indexOf("->")) {
                out = `<b>Why?</b>\nThe store statement updates the field edge of the target abstract node.\n`;
            } else {
                out = `<b>Why?</b>\nThe assignment copies pointer targets.\n`;
            }
        }
    } else if (ev.type === 'WorklistPush') {
        out = `<b>Why?</b>\nState changed entering Block ${ev.blockId}, or it is initial. Added to worklist for dataflow evaluation.\n`;
    } else if (ev.type === 'BlockPop') {
        out = `<b>Why?</b>\nThe solver extracts the next block from the worklist for sequential evaluation.\n`;
    } else if (ev.type === 'Canonicalize') {
        if (ev.isNew) {
            out = `<b>Why?</b>\nNo existing structural match was found. A new canonical index was generated.\n`;
        } else {
            out = `<b>Why?</b>\nAn identical encoded representation already exists and is reused.\n`;
        }
    }
    document.getElementById('causalityView').innerHTML = out;
}

// Helpers
function escapeHtml(str) {
    if (!str) return '';
    return str.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;").replace(/"/g, "&quot;");
}

function formatState(s) {
    if (!s) return "Empty State";
    let out = "";
    out += "Nodes: [" + (s.nodes || []).join(", ") + "]\n\n";
    out += "Bindings:\n";
    if (s.bindings) {
        for (const [k, v] of Object.entries(s.bindings)) {
            out += `  ${k} -> [${(v || []).join(", ")}]\n`;
        }
    }
    out += "\nEdges:\n";
    if (s.edges) {
        s.edges.forEach(e => {
            out += `  ${e.source} -${e.field}-> ${e.target}\n`;
        });
    }
    return out;
}

function formatStateBrief(s) {
    if (!s) return "No state";
    let n = (s.nodes || []).length;
    let e = (s.edges || []).length;
    let b = Object.keys(s.bindings || {}).length;
    return `Nodes: ${n}, Edges: ${e}, Bindings: ${b}`;
}

function computeDiff(s1, s2) {
    if (!s1 && !s2) return "No changes.";
    if (!s1) return "+ All elements initialized.";
    if (!s2) return "- All elements cleared.";
    
    let diff = "";
    let n1 = new Set(s1.nodes || []);
    let n2 = new Set(s2.nodes || []);
    
    n2.forEach(n => { if (!n1.has(n)) diff += `<span class="diff-add">+ Node ${n}</span>\n`; });
    n1.forEach(n => { if (!n2.has(n)) diff += `<span class="diff-sub">- Node ${n}</span>\n`; });
    
    let e1 = new Set((s1.edges || []).map(e => `${e.source}-${e.field}->${e.target}`));
    let e2 = new Set((s2.edges || []).map(e => `${e.source}-${e.field}->${e.target}`));
    
    e2.forEach(e => { if (!e1.has(e)) diff += `<span class="diff-add">+ Edge ${e}</span>\n`; });
    e1.forEach(e => { if (!e2.has(e)) diff += `<span class="diff-sub">- Edge ${e}</span>\n`; });

    return diff || "This event does not modify the HRA state.";
}

// Navigation button handlers
document.getElementById('btnFirst').addEventListener('click', () => {
    if (viewMode === 'correlation') goToCorrStep(1);
    else goToStep(0);
});
document.getElementById('btnPrev').addEventListener('click', () => {
    if (viewMode === 'correlation') goToCorrStep(currentCorrStep - 1);
    else goToStep(currentStep - 1);
});
document.getElementById('btnStep').addEventListener('click', () => {
    if (viewMode === 'correlation') goToCorrStep(currentCorrStep + 1);
    else goToStep(currentStep + 1);
});
document.getElementById('btnNext').addEventListener('click', () => {
    if (viewMode === 'correlation') goToCorrStep(currentCorrStep + 1);
    else goToStep(currentStep + 1);
});
document.getElementById('btnLast').addEventListener('click', () => {
    if (viewMode === 'correlation') goToCorrStep(maxCorrStep);
    else goToStep(maxStep);
});
document.getElementById('btnReset').addEventListener('click', () => {
    if (viewMode === 'correlation') goToCorrStep(1);
    else goToStep(0);
});
document.getElementById('btnRun').addEventListener('click', () => {
    if (viewMode === 'correlation') goToCorrStep(maxCorrStep);
    else goToStep(maxStep);
});

// ============================================================
// PHASE 15: PERFORMANCE DASHBOARD
// ============================================================

function loadPerfData() {
    const statusEl = document.getElementById('perfLoadStatus');
    statusEl.innerText = 'Loading…';
    fetch('performance_data.json?' + Date.now())
        .then(r => {
            if (!r.ok) throw new Error('HTTP ' + r.status);
            return r.json();
        })
        .then(data => {
            perfData = data;
            statusEl.innerText = '✓ Loaded (' + Object.keys(data.benchmarks || {}).length + ' benchmarks)';
            statusEl.style.color = '#4ec9b0';
            // Show tab strip always when perf data is available
            document.getElementById('viewTabs').style.display = 'flex';
            renderPerfDashboard();
            renderAllBmTable();
        })
        .catch(err => {
            statusEl.innerText = '⚠ ' + err.message + ' — run experiments/run_phase15.py first';
            statusEl.style.color = '#f48771';
        });
}

function onBmSelect() {
    const sel = document.getElementById('bmSelector').value;
    if (sel) { selectedBm = sel; renderPerfDashboard(); }
}

// Simple CSS bar chart helper
function barChart(values, labels, colors, maxVal) {
    if (!maxVal || maxVal === 0) maxVal = Math.max(...values.filter(v => v > 0), 1);
    let html = '';
    values.forEach((v, i) => {
        const h = Math.max(4, Math.round(100 * v / maxVal));
        const display = typeof v === 'number' ? (v < 10 ? v.toFixed(3) : v.toFixed(1)) : v;
        html += `<div style="display:flex;flex-direction:column;align-items:center;gap:4px;flex:1;">
            <div style="font-size:0.75em;color:#aaa;">${display}</div>
            <div style="width:100%;background:${colors[i]};height:${h}px;border-radius:3px 3px 0 0;min-height:4px;"></div>
            <div style="font-size:0.8em;color:#888;">${labels[i]}</div>
        </div>`;
    });
    return html;
}

function pctDiff(base, cmp) {
    if (!base || base === 0) return '<span style="color:#888">N/A</span>';
    const pct = 100 * (base - cmp) / base;
    const color = pct > 0 ? '#4ec9b0' : '#f48771';
    const sign  = pct > 0 ? '+' : '';
    return `<span style="color:${color};font-weight:bold;">${sign}${pct.toFixed(1)}%</span>`;
}

function renderPerfDashboard() {
    if (!perfData || !perfData.benchmarks) return;
    const bmData = perfData.benchmarks[selectedBm];
    if (!bmData) return;

    // Update selector to match
    document.getElementById('bmSelector').value = selectedBm;
    document.getElementById('perfBmTitle').innerText = selectedBm + ' — ' + (bmData.classification || '');

    const cfgA = bmData.configs['A'] || {};
    const cfgB = bmData.configs['B'] || {};
    const cfgC = bmData.configs['C'] || {};

    const bcOk = bmData.bc_match;
    const bcCard = document.getElementById('bcStatusContent');
    if (bcOk) {
        bcCard.innerHTML = '<span style="color:#4ec9b0;">✓ PASS</span><div style="font-size:0.4em;color:#888;margin-top:6px;">CanonicalResult(B) == CanonicalResult(C)</div>';
    } else if (bcOk === false) {
        bcCard.innerHTML = '<span style="color:#f48771;">✗ FAIL</span><div style="font-size:0.4em;color:#f48771;margin-top:6px;">B ≠ C — correctness violation!</div>';
    } else {
        bcCard.innerHTML = '<span style="color:#888;">N/A</span>';
    }

    // Time bar chart
    const tA = cfgA.analysis_time_ms_median || 0;
    const tB = cfgB.analysis_time_ms_median || 0;
    const tC = cfgC.analysis_time_ms_median || 0;
    document.getElementById('perfTimeChart').innerHTML = barChart(
        [tA, tB, tC], ['A', 'B', 'C'],
        ['#569cd6', '#4ec9b0', '#ce9178'],
        Math.max(tA, tB, tC)
    );

    // State metrics bar chart (peak heap nodes)
    const nA = cfgA.peak_heap_nodes || 0;
    const nB = cfgB.peak_heap_nodes || 0;
    const nC = cfgC.peak_heap_nodes || 0;
    document.getElementById('perfStateChart').innerHTML = barChart(
        [nA, nB, nC], ['A heap', 'B heap', 'C heap'],
        ['#569cd6', '#4ec9b0', '#ce9178'],
        Math.max(nA, nB, nC)
    );

    // Detailed comparison table
    const na = v => (v === undefined || v === null) ? '<span style="color:#555">—</span>' : v;
    const fms = v => typeof v === 'number' ? v.toFixed(3) : na(v);

    const rows = [
        ['Analysis time (ms) [median]', fms(cfgA.analysis_time_ms_median), fms(cfgB.analysis_time_ms_median), fms(cfgC.analysis_time_ms_median),
         pctDiff(tA, tB), pctDiff(tB, tC)],
        ['  P25 (ms)', fms(cfgA.analysis_time_ms_p25), fms(cfgB.analysis_time_ms_p25), fms(cfgC.analysis_time_ms_p25), '', ''],
        ['  P75 (ms)', fms(cfgA.analysis_time_ms_p75), fms(cfgB.analysis_time_ms_p75), fms(cfgC.analysis_time_ms_p75), '', ''],
        ['Peak memory (KB) [proc WS]', na(cfgA.peak_memory_kb), na(cfgB.peak_memory_kb), na(cfgC.peak_memory_kb),
         '<span style="color:#888;font-size:0.85em;">process-wide</span>', ''],
        ['---', '', '', '', '', ''],
        ['Worklist iterations', na(cfgA.worklist_iterations), na(cfgB.worklist_iterations), na(cfgC.worklist_iterations),
         pctDiff(cfgA.worklist_iterations, cfgB.worklist_iterations),
         pctDiff(cfgB.worklist_iterations, cfgC.worklist_iterations)],
        ['Transfer ops', na(cfgA.transfers_executed), na(cfgB.transfers_executed), na(cfgC.transfers_executed),
         pctDiff(cfgA.transfers_executed, cfgB.transfers_executed),
         pctDiff(cfgB.transfers_executed, cfgC.transfers_executed)],
        ['  allocs', na(cfgA.allocs_processed), na(cfgB.allocs_processed), na(cfgC.allocs_processed), '', ''],
        ['  assigns', na(cfgA.assigns_processed), na(cfgB.assigns_processed), na(cfgC.assigns_processed), '', ''],
        ['  loads', na(cfgA.loads_processed), na(cfgB.loads_processed), na(cfgC.loads_processed), '', ''],
        ['  stores', na(cfgA.stores_processed), na(cfgB.stores_processed), na(cfgC.stores_processed), '', ''],
        ['Peak worklist size', na(cfgA.peak_worklist_size), na(cfgB.peak_worklist_size), na(cfgC.peak_worklist_size), '', ''],
        ['Peak abstract heap nodes', na(cfgA.peak_heap_nodes), na(cfgB.peak_heap_nodes), na(cfgC.peak_heap_nodes),
         pctDiff(cfgA.peak_heap_nodes, cfgB.peak_heap_nodes),
         pctDiff(cfgB.peak_heap_nodes, cfgC.peak_heap_nodes)],
        ['Final heap nodes', na(cfgA.final_heap_nodes), na(cfgB.final_heap_nodes), na(cfgC.final_heap_nodes), '', ''],
        ['Stmts examined', na(cfgA.stmts_examined), na(cfgB.stmts_examined), na(cfgC.stmts_examined), '', ''],
        ['Stmts pruned (dead)', na(cfgA.stmts_pruned), na(cfgB.stmts_pruned), na(cfgC.stmts_pruned),
         pctDiff(cfgA.stmts_pruned === 0 ? -1 : -(cfgA.stmts_pruned - cfgB.stmts_pruned), 0), ''],
        ['Bindings removed', na(cfgA.bindings_removed), na(cfgB.bindings_removed), na(cfgC.bindings_removed), '', ''],
        ['---', '', '', '', '', ''],
        ['MDE cache hits', '<span style="color:#555">N/A</span>', '<span style="color:#555">N/A</span>', na(cfgC.mde_hits), '', ''],
        ['MDE cache misses', '<span style="color:#555">N/A</span>', '<span style="color:#555">N/A</span>', na(cfgC.mde_misses), '', ''],
        ['MDE unique states', '<span style="color:#555">N/A</span>', '<span style="color:#555">N/A</span>', na(cfgC.mde_unique_states), '', ''],
        ['---', '', '', '', '', ''],
        ['B == C (canonical)', '<span style="color:#555">ref</span>', '<span style="color:#4ec9b0;">source</span>',
         bcOk ? '<span style="color:#4ec9b0;font-weight:bold;">✓ EQUAL</span>' : (bcOk === false ? '<span style="color:#f48771;font-weight:bold;">✗ NOT EQUAL</span>' : '<span style="color:#888">N/A</span>'),
         '', ''],
    ];

    let tbody = '';
    rows.forEach(r => {
        if (r[0] === '---') {
            tbody += `<tr><td colspan="6" style="border-top:2px solid #3c3c3c;padding:0;"></td></tr>`;
            return;
        }
        const isSub = r[0].startsWith('  ');
        tbody += `<tr>
            <td style="${isSub ? 'color:#888;padding-left:20px;' : 'font-weight:bold;'}">${r[0]}</td>
            <td style="color:#9cdcfe;">${r[1]}</td>
            <td style="color:#4ec9b0;">${r[2]}</td>
            <td style="color:#ce9178;">${r[3]}</td>
            <td style="text-align:right;">${r[4]}</td>
            <td style="text-align:right;">${r[5]}</td>
        </tr>`;
    });
    document.getElementById('perfCompBody').innerHTML = tbody;

    // MDE Analysis Panel
    const hitRate = (cfgC.mde_hits || 0) + (cfgC.mde_misses || 0);
    const hitPct = hitRate > 0 ? (100 * cfgC.mde_hits / hitRate).toFixed(1) : 'N/A';
    const safe = bmData.safe_for_runtime;
    document.getElementById('mdeAnalysisView').innerHTML = `
        <div class="kv-pair"><span class="kv-key">Benchmark</span><span class="kv-val" style="color:#ce9178;">${selectedBm}</span></div>
        <div class="kv-pair"><span class="kv-key">Runtime Safety</span><span class="kv-val">${safe ? '<span class="badge badge-live">SAFE</span>' : '<span class="badge badge-pruned">UB/UNSAFE</span>'}</span></div>
        <div class="kv-pair"><span class="kv-key">MDE Hits</span><span class="kv-val" style="color:#4ec9b0;font-size:1.2em;font-weight:bold;">${cfgC.mde_hits || 0}</span></div>
        <div class="kv-pair"><span class="kv-key">MDE Misses (new states)</span><span class="kv-val">${cfgC.mde_misses || 0}</span></div>
        <div class="kv-pair"><span class="kv-key">Unique states memoized</span><span class="kv-val">${cfgC.mde_unique_states || 0}</span></div>
        <div class="kv-pair"><span class="kv-key">Cache Hit Rate</span><span class="kv-val" style="color:${parseFloat(hitPct) > 50 ? '#4ec9b0' : '#e5c07b'};font-weight:bold;">${hitPct}%</span></div>
        <hr style="border-color:#333;">
        <div style="color:#888;font-size:0.85em;">
            MDE deduplication enables <strong>O(1)</strong> fixpoint check via pointer equality
            instead of <strong>O(P×N)</strong> deep state comparison.<br>
            Higher hit rate → more fixpoint iterations avoided.<br><br>
            <strong style="color:#ce9178;">Correctness:</strong> MDE does NOT affect the analysis result.<br>
            CanonicalResult(B) == CanonicalResult(C) = <strong style="color:${bcOk ? '#4ec9b0' : '#f48771'}">${bcOk ? 'VERIFIED' : 'FAILED'}</strong>
        </div>
        ${bmData.corr_trace ? `<div style="margin-top:10px;"><button onclick="loadCorrTrace('${bmData.corr_trace}')" style="font-size:0.85em;">Load Correlation Trace →</button></div>` : ''}
    `;
}

function renderAllBmTable() {
    if (!perfData || !perfData.benchmarks) return;
    let tbody = '';
    Object.entries(perfData.benchmarks).forEach(([bm, bmd]) => {
        const cfgA = bmd.configs['A'] || {};
        const cfgB = bmd.configs['B'] || {};
        const cfgC = bmd.configs['C'] || {};
        const bc = bmd.bc_match;
        const bcStr = bc === true ? '<span style="color:#4ec9b0;">✓ PASS</span>'
                    : bc === false ? '<span style="color:#f48771;">✗ FAIL</span>'
                    : '<span style="color:#888;">N/A</span>';
        const safe = bmd.safe_for_runtime ? '' : ' <span class="badge badge-pruned" style="font-size:0.7em;">UB/unsafe</span>';
        tbody += `<tr onclick="selectedBm='${bm}';renderPerfDashboard();" style="cursor:pointer;" title="Click to inspect ${bm}">
            <td style="color:#569cd6;font-weight:bold;">${bm}</td>
            <td style="font-size:0.85em;">${bmd.classification || ''}${safe}</td>
            <td>${bcStr}</td>
            <td>${cfgA.transfers_executed ?? '—'}</td>
            <td>${cfgB.transfers_executed ?? '—'}</td>
            <td>${cfgC.transfers_executed ?? '—'}</td>
            <td style="color:#ce9178;">${cfgC.mde_hits ?? '—'}</td>
            <td>${typeof cfgA.analysis_time_ms_median === 'number' ? cfgA.analysis_time_ms_median.toFixed(3) : '—'}</td>
            <td>${typeof cfgC.analysis_time_ms_median === 'number' ? cfgC.analysis_time_ms_median.toFixed(3) : '—'}</td>
        </tr>`;
    });
    document.getElementById('perfAllBmBody').innerHTML = tbody || '<tr><td colspan="9" style="color:#888;">No data</td></tr>';
}

function loadCorrTrace(filename) {
    fetch(filename)
        .then(r => r.json())
        .then(data => {
            traceData = data;
            mermaid.initialize({ startOnLoad: false, theme: 'dark', maxTextSize: 90000 });
            document.getElementById('traceControls').style.display = 'flex';
            document.getElementById('pipeline-indicator').style.display = 'flex';
            document.getElementById('currentEventPanel').style.display = 'block';
            switchViewMode('correlation');
            initCorrelationVisualization();
        })
        .catch(err => alert('Failed to load ' + filename + ': ' + err.message));
}

// Auto-load performance dashboard on page load
window.addEventListener('DOMContentLoaded', () => {
    // Show tabs immediately with perf tab active
    document.getElementById('viewTabs').style.display = 'flex';
    document.getElementById('perfPanels').style.display = 'flex';
    loadPerfData();
});
