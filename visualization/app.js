let traceData = null;
let currentStep = 0;
let maxStep = 0;

let stateAtStep = [];
let queueAtStep = [];
let cfgZoom = null;
let hraZoom = null;

document.getElementById('traceFile').addEventListener('change', function(e) {
    const file = e.target.files[0];
    if (!file) return;
    
    const reader = new FileReader();
    reader.onload = function(e) {
        try {
            traceData = JSON.parse(e.target.result);
            document.getElementById('loadStatus').innerText = "Loaded " + traceData.config + " Config";
            document.getElementById('traceControls').style.display = 'flex';
            document.getElementById('pipeline-indicator').style.display = 'flex';
            document.getElementById('currentEventPanel').style.display = 'block';
            document.getElementById('mainPanels').style.display = 'block';
            mermaid.initialize({ startOnLoad: false, theme: 'dark', maxTextSize: 90000 });
            initVisualization();
        } catch (err) {
            document.getElementById('loadStatus').innerText = "Error parsing JSON: " + err.message;
            document.getElementById('loadStatus').style.color = "red";
        }
    };
    reader.readAsText(file);
});

function initVisualization() {
    maxStep = traceData.trace.length;
    
    // Precompute state and queue arrays
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
    
    // Build raw event log
    const el = document.getElementById('eventLog');
    el.innerHTML = '';
    // Push step 0 explicitly
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
            cfgHtml += `<span class="badge"></span>`;
            cfgHtml += `</div>`;
        });
        cfgHtml += `</div>`;
    }
    document.getElementById('cfgView').innerHTML = cfgHtml;
}

async function renderCFGGraph() {
    let m = "graph TD\n";
    for (const [blockId, block] of Object.entries(traceData.cfg)) {
        m += `    B${blockId}["Block ${blockId}"]\n`;
        block.successors.forEach(succ => {
            if (succ <= parseInt(blockId)) {
                m += `    B${blockId} -->|BACK-EDGE| B${succ}\n`;
                m += `    style B${blockId} stroke:#f48771,stroke-width:2px\n`;
            } else {
                m += `    B${blockId} --> B${succ}\n`;
            }
        });
    }
    m += `    classDef activeBlock fill:#094771,stroke:#569cd6,stroke-width:3px,color:#fff;\n`;
    try {
        let res = await mermaid.render('cfg_graph_full', m);
        let container = document.getElementById('mermaid-cfg');
        container.innerHTML = res.svg;
        let svgEl = container.querySelector('svg');
        svgEl.style.width = '100%';
        svgEl.style.height = '100%';
        if (cfgZoom) { cfgZoom.destroy(); }
        cfgZoom = svgPanZoom(svgEl, { controlIconsEnabled: false, zoomEnabled: true, panEnabled: true, fit: true, center: true });
    } catch(e) {
        document.getElementById('mermaid-cfg').innerHTML = "Error rendering CFG";
    }
}

function escapeHtml(unsafe) {
    if (!unsafe) return "";
    return unsafe.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;").replace(/"/g, "&quot;").replace(/'/g, "&#039;");
}

function setStage(stage) {
    document.querySelectorAll('.stage').forEach(el => el.classList.remove('active'));
    if (stage) document.getElementById('stage-' + stage).classList.add('active');
}

function updateNavigationControls() {
    document.getElementById('btnFirst').disabled = (currentStep === 0);
    document.getElementById('btnPrev').disabled = (currentStep === 0);
    document.getElementById('btnStep').disabled = (currentStep === maxStep);
    document.getElementById('btnNext').disabled = (currentStep === maxStep);
    document.getElementById('btnLast').disabled = (currentStep === maxStep);
    document.getElementById('btnRun').disabled = (currentStep === maxStep);
    document.getElementById('stepCounter').innerText = `Step: ${currentStep} / ${maxStep}`;
}

function goToStep(step) {
    if (step < 0) step = 0;
    if (step > maxStep) step = maxStep;
    currentStep = step;
    updateAllPanels();
}

function updateAllPanels() {
    updateNavigationControls();
    
    // Highlight Event Log
    document.querySelectorAll('.log-entry').forEach(e => e.classList.remove('active'));
    let activeLog = document.getElementById(`log-${currentStep}`);
    if (activeLog) {
        activeLog.classList.add('active');
        activeLog.scrollIntoView({behavior: "smooth", block: "nearest"});
    }

    if (currentStep === 0) {
        renderInitialState();
        return;
    }
    
    const ev = traceData.trace[currentStep - 1];
    const state = stateAtStep[currentStep];
    const oldState = stateAtStep[currentStep - 1];
    const qBefore = queueAtStep[currentStep - 1];
    const qAfter = queueAtStep[currentStep];
    
    // 1. Pipeline & Block highlighting
    document.querySelectorAll('.cfg-node.active, .cfg-stmt.active').forEach(n => n.classList.remove('active'));
    if (ev.blockId !== undefined) {
        let bn = document.getElementById(`cfg-block-${ev.blockId}`);
        if (bn) { bn.classList.add('active'); bn.scrollIntoView({behavior: "smooth", block: "nearest"}); }
        
        // Mermaid highlight
        if (cfgZoom) {
            let svg = document.querySelector('#mermaid-cfg svg');
            if (svg) {
                svg.querySelectorAll('.node rect').forEach(r => r.style.fill = '');
                let activeRect = svg.querySelector(`[id^="flowchart-B${ev.blockId}-"] rect`);
                if (activeRect) activeRect.style.fill = '#094771';
            }
        }
    }
    
    if (ev.stmt) {
        let stmts = document.querySelectorAll(`.cfg-stmt`);
        for (let s of stmts) {
            if (s.getAttribute('data-stmt') === ev.stmt) {
                s.classList.add('active');
                if (ev.type === 'StmtPost') {
                    if (ev.pruned) {
                        s.classList.add('pruned');
                        let b = s.querySelector('.badge'); b.className = 'badge badge-pruned'; b.innerText = 'PRUNED';
                    } else {
                        s.classList.remove('pruned');
                        let b = s.querySelector('.badge'); b.className = 'badge badge-live'; b.innerText = 'LIVE';
                    }
                }
                break;
            }
        }
    }
    
    // 2. State & Diff & Mermaid HRA
    renderState(state, oldState, ev);
    
    // 3. Worklist
    renderWorklist(ev, qBefore, qAfter);
    
    // 4. Liveness
    let livenessInfo = null;
    if (ev.stmt) {
        livenessInfo = renderLiveness(ev.stmt);
    } else {
        document.getElementById('livenessView').innerHTML = "No statement-level liveness event at this trace position.";
    }
    
    // 5. MDE
    renderMDE(ev);
    
    // 6. Current Event & Causality
    updateCurrentEventInspector(ev, oldState, state, livenessInfo);
    updateCausality(ev, livenessInfo);
    
    // 7. Pipeline stage
    if (ev.type === 'BlockPop' || ev.type === 'WorklistPush') setStage('fixpoint');
    else if (ev.type === 'StmtPre') setStage('liveness');
    else if (ev.type === 'StmtPost' || ev.type === 'BlockEntry' || ev.type === 'BlockExit') setStage('hra');
    else if (ev.type === 'Canonicalize') setStage('mde');
}

function renderInitialState() {
    setStage('');
    document.getElementById('ce-content').innerHTML = `
        <div class="kv-pair"><span class="kv-key">Status:</span><span class="kv-val">INITIAL STATE</span></div>
        <div class="kv-pair"><span class="kv-val">No events have executed yet.</span></div>
    `;
    document.getElementById('graphView').innerHTML = "HRA state not yet available: no HRA transfer has executed at this trace position.";
    document.getElementById('stateDiffView').innerHTML = "No previous state.";
    document.getElementById('mermaid-hra').innerHTML = "";
    document.getElementById('livenessView').innerHTML = "No statement-level liveness event at this trace position.";
    document.getElementById('worklistView').innerHTML = "Queue empty.";
    document.getElementById('mdeLog').innerHTML = "No MDE event at this trace position.";
    document.getElementById('causalityView').innerHTML = "No event selected.";
    if (hraZoom) { hraZoom.destroy(); hraZoom = null; }
    
    document.querySelectorAll('.cfg-node.active, .cfg-stmt.active').forEach(n => n.classList.remove('active'));
    document.querySelectorAll('.cfg-stmt .badge').forEach(n => { n.className = 'badge'; n.innerText = ''; });
    document.querySelectorAll('.cfg-stmt').forEach(n => n.classList.remove('pruned'));
}

function computeDiff(before, after) {
    if (!before || !after) return "Not available from trace.";
    let diff = "";
    
    // Bindings
    let allVars = new Set([...Object.keys(before.bindings), ...Object.keys(after.bindings)]);
    for (let v of allVars) {
        let bNodes = new Set(before.bindings[v] || []);
        let aNodes = new Set(after.bindings[v] || []);
        let added = [...aNodes].filter(x => !bNodes.has(x));
        let removed = [...bNodes].filter(x => !aNodes.has(x));
        if (added.length > 0) diff += `<span class="diff-add">+ binding: ${v} -> {${added.join(', ')}}</span>\n`;
        if (removed.length > 0) diff += `<span class="diff-sub">- binding: ${v} -> {${removed.join(', ')}}</span>\n`;
    }
    
    // Nodes
    let bNodes = new Set(before.nodes);
    let aNodes = new Set(after.nodes);
    let addedN = [...aNodes].filter(x => !bNodes.has(x));
    let removedN = [...bNodes].filter(x => !aNodes.has(x));
    if (addedN.length > 0) diff += `<span class="diff-add">+ nodes: {${addedN.join(', ')}}</span>\n`;
    if (removedN.length > 0) diff += `<span class="diff-sub">- nodes: {${removedN.join(', ')}}</span>\n`;
    
    // Edges
    let eToStr = e => `N${e.source} --${e.field}--> N${e.target}`;
    let bEdges = new Set(before.edges.map(eToStr));
    let aEdges = new Set(after.edges.map(eToStr));
    let addedE = [...aEdges].filter(x => !bEdges.has(x));
    let removedE = [...bEdges].filter(x => !aEdges.has(x));
    
    addedE.forEach(e => diff += `<span class="diff-add">+ edge: ${e}</span>\n`);
    removedE.forEach(e => diff += `<span class="diff-sub">- edge: ${e}</span>\n`);
    
    return diff === "" ? "This event does not modify the HRA state." : diff;
}

function renderState(stateObj, oldStateObj, ev) {
    if (!stateObj) {
        document.getElementById('graphView').innerHTML = "HRA state not yet available: no HRA transfer has executed at this trace position.";
        document.getElementById('stateDiffView').innerHTML = "Not available.";
        document.getElementById('mermaid-hra').innerHTML = "";
        if (hraZoom) { hraZoom.destroy(); hraZoom = null; }
        return;
    }
    
    if (stateObj.nodes.length === 0 && Object.keys(stateObj.bindings).length === 0) {
        document.getElementById('graphView').innerHTML = "Current HRA graph is genuinely empty.";
        document.getElementById('mermaid-hra').innerHTML = "Empty graph.";
        if (hraZoom) { hraZoom.destroy(); hraZoom = null; }
    } else {
        let out = "Bindings:\n";
        for (const [v, nodes] of Object.entries(stateObj.bindings)) {
            out += `    ${v} -> {${nodes.join(', ')}}\n`;
        }
        out += "\nGraph Nodes:\n    {" + stateObj.nodes.join(', ') + "}\n";
        out += "\nHeap Edges:\n";
        stateObj.edges.forEach(e => { out += `    N${e.source} --${e.field}--> N${e.target}\n`; });
        document.getElementById('graphView').innerHTML = escapeHtml(out);
        
        let m = "graph TD\n";
        for (const [v, nodes] of Object.entries(stateObj.bindings)) {
            m += `    var_${v}("${v}"):::varNode\n`;
            nodes.forEach(n => { m += `    var_${v} --> N${n}\n`; });
        }
        stateObj.nodes.forEach(n => { m += `    N${n}(("N${n}")):::heapNode\n`; });
        stateObj.edges.forEach(e => { m += `    N${e.source} -->|${e.field}| N${e.target}\n`; });
        m += `    classDef varNode fill:#094771,stroke:#333,stroke-width:2px,color:#fff;\n`;
        m += `    classDef heapNode fill:#2d2d30,stroke:#569cd6,stroke-width:2px,color:#fff;\n`;
        
        mermaid.render('hra_graph_render_' + currentStep, m).then(res => {
            let container = document.getElementById('mermaid-hra');
            container.innerHTML = res.svg;
            let svgEl = container.querySelector('svg');
            svgEl.style.width = '100%';
            svgEl.style.height = '100%';
            if (hraZoom) { hraZoom.destroy(); }
            hraZoom = svgPanZoom(svgEl, { controlIconsEnabled: false, zoomEnabled: true, panEnabled: true, fit: true, center: true });
        }).catch(e => {
            document.getElementById('mermaid-hra').innerHTML = "Error rendering heap graph.";
        });
    }

    let diffText = computeDiff(oldStateObj, stateObj);
    if (!ev.state) { // Event did not explicitly emit a state change
        diffText = "This event does not modify the HRA state.";
    }
    
    document.getElementById('stateDiffView').innerHTML = `<b>BEFORE:</b>\n${formatStateBrief(oldStateObj)}\n\n<b>EVENT:</b>\n${ev.type} ${ev.stmt || ''}\n\n<b>AFTER:</b>\n${formatStateBrief(stateObj)}\n\n<b>CHANGES:</b>\n${diffText}`;
}

function formatStateBrief(stateObj) {
    if (!stateObj) return "Not available.";
    let parts = [];
    for (const [v, nodes] of Object.entries(stateObj.bindings)) parts.push(`${v} -> {${nodes.join(',')}}`);
    if (parts.length === 0) return "Empty bindings";
    return parts.join("\n");
}

function renderLiveness(stmtText) {
    if (!traceData.liveness || !traceData.liveness[stmtText]) {
        document.getElementById('livenessView').innerHTML = "No liveness info available for this statement.";
        return null;
    }
    const l = traceData.liveness[stmtText];
    let out = `<b>Statement:</b>\n    ${escapeHtml(stmtText)}\n\n`;
    out += `<b>USE:</b>\n    {${l.use ? l.use.join(', ') : ''}}\n\n`;
    out += `<b>DEF:</b>\n    {${l.def ? l.def.join(', ') : ''}}\n\n`;
    out += `<b>LiveBefore:</b>\n    {${l.before.join(', ')}}\n\n`;
    out += `<b>LiveAfter:</b>\n    {${l.after.join(', ')}}\n\n`;
    out += `<b>Decision:</b>\n    Evaluated during HRA Transfer\n`;
    document.getElementById('livenessView').innerHTML = out;
    return l;
}

function renderWorklist(ev, qBefore, qAfter) {
    let out = `<b>BEFORE QUEUE:</b>\n    [${qBefore.join(', ')}]\n\n`;
    if (ev.type === 'BlockPop') {
        out += `<b>EVENT:</b>\n    POP ${ev.blockId}\n\n`;
    } else if (ev.type === 'WorklistPush') {
        out += `<b>EVENT:</b>\n    PUSH ${ev.blockId}\n\n`;
    } else {
        out += `<b>EVENT:</b>\n    (No queue operation)\n\n`;
    }
    out += `<b>AFTER QUEUE:</b>\n    [${qAfter.join(', ')}]\n`;
    document.getElementById('worklistView').innerHTML = out;
}

function renderMDE(ev) {
    if (ev.type === 'Canonicalize') {
        let out = `MDE LOOKUP\n    ↓\n`;
        if (ev.isNew) {
            out += `MISS\n    ↓\nAction: Create new canonical representation\n`;
        } else {
            out += `HIT\n    ↓\nExisting index: #${ev.mdeIndex}\n    ↓\nAction: Reuse canonical representation\n`;
        }
        document.getElementById('mdeLog').innerHTML = out;
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
        let block = traceData.cfg[ev.blockId];
        let isBackedge = false;
        // In this simple check, if push happens, we look if blockId is <= any pred to guess backedge...
        // Actually CFG backedge is when current executing block (or just finished) >= succ.
        // It's a back-edge traversal if blockId <= current active block (but we pop'ed it, so let's rely on event logic).
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

document.getElementById('btnFirst').addEventListener('click', () => goToStep(0));
document.getElementById('btnPrev').addEventListener('click', () => goToStep(currentStep - 1));
document.getElementById('btnStep').addEventListener('click', () => goToStep(currentStep + 1));
document.getElementById('btnNext').addEventListener('click', () => goToStep(currentStep + 1));
document.getElementById('btnLast').addEventListener('click', () => goToStep(maxStep));
document.getElementById('btnReset').addEventListener('click', () => goToStep(0));
document.getElementById('btnRun').addEventListener('click', () => goToStep(maxStep));

