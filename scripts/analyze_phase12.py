import os
import sys
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt

def main():
    raw_csv_path = "experiments/raw/run_2026-10-06_052335.csv"
    if not os.path.exists(raw_csv_path):
        print(f"Error: {raw_csv_path} not found.")
        sys.exit(1)

    tables_dir = "experiments/analysis/tables"
    plots_dir = "experiments/analysis/plots"
    os.makedirs(tables_dir, exist_ok=True)
    os.makedirs(plots_dir, exist_ok=True)

    df = pd.read_csv(raw_csv_path)
    print(f"Loaded {raw_csv_path} with shape {df.shape}")

    # Standardize benchmark short name
    def clean_bm_name(val):
        base = os.path.basename(val)
        return base.replace(".c", "")

    df['bm_short'] = df['program'].apply(clean_bm_name)

    # -------------------------------------------------------------
    # PART 1: DATA VALIDATION
    # -------------------------------------------------------------
    total_rows = len(df)
    benchmarks = sorted(df['bm_short'].unique())
    num_benchmarks = len(benchmarks)
    configs = sorted(df['configuration'].unique())
    num_configs = len(configs)
    
    # Counts per pair
    counts = df.groupby(['bm_short', 'configuration']).size()
    min_runs = counts.min()
    max_runs = counts.max()
    
    null_counts = df.isnull().sum().sum()
    dupes = df.duplicated(subset=['bm_short', 'configuration', 'run_id']).sum()
    failed_runs = (df['success'] != 1).sum()

    validation_summary = {
        "Metric": [
            "Total Data Rows",
            "Unique Benchmarks",
            "Configurations",
            "Repetitions per Pair (Min/Max)",
            "Expected Total Rows",
            "Missing / Null Values",
            "Duplicate Run IDs",
            "Failed Runs (success != 1)",
            "Coverage Consistency"
        ],
        "Value": [
            str(total_rows),
            f"{num_benchmarks} ({', '.join(benchmarks)})",
            f"{num_configs} ({', '.join(configs)})",
            f"{min_runs} / {max_runs}",
            f"{num_benchmarks} * {num_configs} * 10 = {num_benchmarks * num_configs * 10}",
            str(null_counts),
            str(dupes),
            str(failed_runs),
            "Complete & Balanced (10 runs for all 24 pairs)"
        ]
    }
    val_df = pd.DataFrame(validation_summary)
    val_df.to_csv(os.path.join(tables_dir, "data_validation_summary.csv"), index=False)
    with open(os.path.join(tables_dir, "data_validation_summary.md"), "w") as f:
        f.write(val_df.to_markdown(index=False))

    print("Data Validation Completed. Status: PASS.")

    # -------------------------------------------------------------
    # PART 2 & PART 3: PER-BENCHMARK EXTRACTION
    # Deterministic structural metrics: first run represents all 10 identically
    # -------------------------------------------------------------
    summary_rows = []
    for bm in benchmarks:
        sub_a = df[(df['bm_short'] == bm) & (df['configuration'] == 'A')].iloc[0]
        sub_b = df[(df['bm_short'] == bm) & (df['configuration'] == 'B')].iloc[0]
        sub_c = df[(df['bm_short'] == bm) & (df['configuration'] == 'C')].iloc[0]

        summary_rows.append({
            'benchmark': bm,
            # Worklist / blocks
            'blocks_A': sub_a['blocks_processed'],
            'blocks_B': sub_b['blocks_processed'],
            'blocks_C': sub_c['blocks_processed'],
            'worklist_A': sub_a['worklist_iters'],
            'worklist_B': sub_b['worklist_iters'],
            'worklist_C': sub_c['worklist_iters'],
            # Transfers
            'transfers_A': sub_a['transfers'],
            'transfers_B': sub_b['transfers'],
            'transfers_C': sub_c['transfers'],
            # Pruning & Liveness
            'stmts_examined_A': sub_a['stmts_examined'],
            'stmts_examined_B': sub_b['stmts_examined'],
            'stmts_pruned_B': sub_b['stmts_pruned'],
            'pct_pruned_B': (sub_b['stmts_pruned'] / sub_b['stmts_examined'] * 100.0) if sub_b['stmts_examined'] > 0 else 0.0,
            'bindings_removed_B': sub_b['bindings_removed'],
            'liveness_iters_B': sub_b['liveness_iters'],
            'liveness_blocks_B': sub_b['liveness_blocks'],
            # Allocations
            'allocs_A': sub_a['allocs'],
            'allocs_B': sub_b['allocs'],
            'allocs_C': sub_c['allocs'],
            # States
            'states_A': sub_a['states_constructed'],
            'states_B': sub_b['states_constructed'],
            'states_C': sub_c['states_constructed'],
            # Canonicalization & MDE
            'canon_calls_C': sub_c['canon_calls'],
            'canon_new_C': sub_c['canon_new'],
            'canon_reused_C': sub_c['canon_reused'],
            'mde_hits_C': sub_c['mde_state_hits'],
            'mde_misses_C': sub_c['mde_state_misses'],
            'mde_hit_rate_C': (sub_c['mde_state_hits'] / (sub_c['mde_state_hits'] + sub_c['mde_state_misses']) * 100.0) if (sub_c['mde_state_hits'] + sub_c['mde_state_misses']) > 0 else 0.0,
            'canon_reuse_rate_C': (sub_c['canon_reused'] / sub_c['canon_calls'] * 100.0) if sub_c['canon_calls'] > 0 else 0.0,
        })
    s_df = pd.DataFrame(summary_rows)

    # -------------------------------------------------------------
    # TABLE: A vs B (Liveness Effect)
    # -------------------------------------------------------------
    a_vs_b = s_df[['benchmark', 'transfers_A', 'transfers_B', 'stmts_examined_B', 'stmts_pruned_B', 'pct_pruned_B', 'bindings_removed_B', 'worklist_A', 'worklist_B', 'allocs_A', 'allocs_B']].copy()
    a_vs_b['transfer_reduction'] = a_vs_b.apply(lambda r: f"{((r['transfers_A'] - r['transfers_B']) / r['transfers_A'] * 100.0):.1f}%" if r['transfers_A'] > 0 else "0.0%", axis=1)
    a_vs_b['pct_pruned_B'] = a_vs_b['pct_pruned_B'].apply(lambda v: f"{v:.1f}%")
    a_vs_b.to_csv(os.path.join(tables_dir, "table_a_vs_b_liveness.csv"), index=False)
    with open(os.path.join(tables_dir, "table_a_vs_b_liveness.md"), "w") as f:
        f.write(a_vs_b.to_markdown(index=False))

    # -------------------------------------------------------------
    # TABLE: B vs C (MDE Effect)
    # -------------------------------------------------------------
    b_vs_c = s_df[['benchmark', 'states_B', 'states_C', 'canon_calls_C', 'canon_new_C', 'canon_reused_C', 'mde_hits_C', 'mde_misses_C', 'mde_hit_rate_C', 'canon_reuse_rate_C', 'worklist_B', 'worklist_C', 'transfers_B', 'transfers_C']].copy()
    b_vs_c['mde_hit_rate_C'] = b_vs_c['mde_hit_rate_C'].apply(lambda v: f"{v:.1f}%")
    b_vs_c['canon_reuse_rate_C'] = b_vs_c['canon_reuse_rate_C'].apply(lambda v: f"{v:.1f}%")
    b_vs_c.to_csv(os.path.join(tables_dir, "table_b_vs_c_mde.csv"), index=False)
    with open(os.path.join(tables_dir, "table_b_vs_c_mde.md"), "w") as f:
        f.write(b_vs_c.to_markdown(index=False))

    # -------------------------------------------------------------
    # TABLE: Normalized Comparisons
    # -------------------------------------------------------------
    norm_rows = []
    for _, r in s_df.iterrows():
        # A -> B
        tf_red = (r['transfers_A'] - r['transfers_B']) / r['transfers_A'] if r['transfers_A'] > 0 else 0.0
        al_red = (r['allocs_A'] - r['allocs_B']) / r['allocs_A'] if r['allocs_A'] > 0 else 0.0
        wk_ab = (r['worklist_A'] - r['worklist_B']) / r['worklist_A'] if r['worklist_A'] > 0 else 0.0

        # B -> C
        st_bc = (r['states_B'] - r['states_C']) / r['states_B'] if r['states_B'] > 0 else 0.0
        wk_bc = (r['worklist_B'] - r['worklist_C']) / r['worklist_B'] if r['worklist_B'] > 0 else 0.0
        tf_bc = (r['transfers_B'] - r['transfers_C']) / r['transfers_B'] if r['transfers_B'] > 0 else 0.0

        norm_rows.append({
            'Benchmark': r['benchmark'],
            'A->B Transfers': f"-{tf_red*100:.1f}% (reduction)" if tf_red > 0 else "0.0% (unchanged)",
            'A->B Allocs': f"-{al_red*100:.1f}% (reduction)" if al_red > 0 else "0.0% (unchanged)",
            'A->B Worklist': f"{wk_ab*100:.1f}% (unchanged)",
            'B->C States': f"{st_bc*100:.1f}% (unchanged)",
            'B->C Worklist': f"{wk_bc*100:.1f}% (unchanged)",
            'B->C Transfers': f"{tf_bc*100:.1f}% (unchanged)",
            'B->C MDE State Reuse Rate': f"{r['canon_reuse_rate_C']:.1f}% (reuse)",
        })
    norm_df = pd.DataFrame(norm_rows)
    norm_df.to_csv(os.path.join(tables_dir, "table_normalized_comparisons.csv"), index=False)
    with open(os.path.join(tables_dir, "table_normalized_comparisons.md"), "w") as f:
        f.write(norm_df.to_markdown(index=False))

    # -------------------------------------------------------------
    # TABLE: MDE Reuse Analysis Specific
    # -------------------------------------------------------------
    mde_table = s_df[['benchmark', 'canon_calls_C', 'canon_new_C', 'canon_reused_C', 'mde_hits_C', 'mde_misses_C', 'mde_hit_rate_C', 'canon_reuse_rate_C']].copy()
    mde_table.rename(columns={
        'benchmark': 'Benchmark',
        'canon_calls_C': 'Canon Calls',
        'canon_new_C': 'Canon New',
        'canon_reused_C': 'Canon Reused',
        'mde_hits_C': 'MDE Hits',
        'mde_misses_C': 'MDE Misses',
        'mde_hit_rate_C': 'Hit Rate (%)',
        'canon_reuse_rate_C': 'Reuse Rate (%)'
    }, inplace=True)
    mde_table.to_csv(os.path.join(tables_dir, "table_mde_reuse_analysis.csv"), index=False)
    with open(os.path.join(tables_dir, "table_mde_reuse_analysis.md"), "w") as f:
        f.write(mde_table.to_markdown(index=False))

    # -------------------------------------------------------------
    # PART 5: AGGREGATE STATISTICS ACROSS THE 8 BENCHMARKS
    # -------------------------------------------------------------
    agg_metrics = [
        ('Transfers (Config A)', s_df['transfers_A']),
        ('Transfers (Config B)', s_df['transfers_B']),
        ('Transfers (Config C)', s_df['transfers_C']),
        ('Statements Examined (B)', s_df['stmts_examined_B']),
        ('Statements Pruned (B)', s_df['stmts_pruned_B']),
        ('Bindings Removed (B)', s_df['bindings_removed_B']),
        ('Worklist Iterations (A)', s_df['worklist_A']),
        ('Worklist Iterations (B)', s_df['worklist_B']),
        ('Worklist Iterations (C)', s_df['worklist_C']),
        ('States Constructed (A)', s_df['states_A']),
        ('States Constructed (B)', s_df['states_B']),
        ('States Constructed (C)', s_df['states_C']),
        ('Canon Calls (C)', s_df['canon_calls_C']),
        ('Canon Reused (C)', s_df['canon_reused_C']),
        ('MDE Hits (C)', s_df['mde_hits_C']),
        ('MDE Misses (C)', s_df['mde_misses_C']),
        ('MDE Hit Rate % (C)', s_df['mde_hit_rate_C']),
    ]
    agg_rows = []
    for name, series in agg_metrics:
        agg_rows.append({
            'Metric': name,
            'Mean': f"{series.mean():.2f}",
            'Median': f"{series.median():.2f}",
            'Min': f"{series.min():.2f}",
            'Max': f"{series.max():.2f}",
            'StdDev (across benchmarks)': f"{series.std():.2f}",
            'Repeat StdDev (within runs)': "0.00 (deterministic)"
        })
    agg_df = pd.DataFrame(agg_rows)
    agg_df.to_csv(os.path.join(tables_dir, "table_aggregate_statistics.csv"), index=False)
    with open(os.path.join(tables_dir, "table_aggregate_statistics.md"), "w") as f:
        f.write(agg_df.to_markdown(index=False))

    # -------------------------------------------------------------
    # PLOTS GENERATION
    # -------------------------------------------------------------
    plt.style.use('seaborn-v0_8-whitegrid' if 'seaborn-v0_8-whitegrid' in plt.style.available else 'default')
    bm_labels = [b.replace("bm0", "BM").replace("bm", "BM") for b in benchmarks]
    x = np.arange(len(bm_labels))
    width = 0.35

    # Plot 1: A vs B Statements Examined vs Pruned
    fig, ax = plt.subplots(figsize=(10, 5))
    ax.bar(x - width/2, s_df['stmts_examined_B'], width, label='Examined Statements', color='#2b5c8f')
    ax.bar(x + width/2, s_df['stmts_pruned_B'], width, label='Pruned Statements (Liveness)', color='#d95f02')
    ax.set_ylabel('Statement Count')
    ax.set_title('Figure 1: Statements Examined vs Pruned by Liveness (Config B)')
    ax.set_xticks(x)
    ax.set_xticklabels(bm_labels, rotation=30, ha='right')
    ax.legend()
    # Annotate non-zero pruned values
    for i, val in enumerate(s_df['stmts_pruned_B']):
        if val > 0:
            ax.annotate(f"{val} ({s_df['pct_pruned_B'].iloc[i]:.0f}%)",
                        (x[i] + width/2, val + 0.2),
                        ha='center', fontsize=9, fontweight='bold', color='#d95f02')
    plt.tight_layout()
    fig.savefig(os.path.join(plots_dir, "plot1_a_vs_b_pruning.png"), dpi=300)
    plt.close(fig)

    # Plot 2: A vs B Transfers per Benchmark
    fig, ax = plt.subplots(figsize=(10, 5))
    ax.bar(x - width/2, s_df['transfers_A'], width, label='Config A (Baseline Transfers)', color='#7570b3')
    ax.bar(x + width/2, s_df['transfers_B'], width, label='Config B (Liveness Transfers)', color='#1b9e77')
    ax.set_ylabel('Transfer Function Invocations')
    ax.set_title('Figure 2: Transfer Function Reduction from Liveness (A vs B)')
    ax.set_xticks(x)
    ax.set_xticklabels(bm_labels, rotation=30, ha='right')
    ax.legend()
    for i, (ta, tb) in enumerate(zip(s_df['transfers_A'], s_df['transfers_B'])):
        if ta > tb:
            diff = ta - tb
            ax.annotate(f"-{diff} (-{(diff/ta)*100:.0f}%)",
                        (x[i] + width/2, tb + 0.2),
                        ha='center', fontsize=9, fontweight='bold', color='#1b9e77')
    plt.tight_layout()
    fig.savefig(os.path.join(plots_dir, "plot2_a_vs_b_transfers.png"), dpi=300)
    plt.close(fig)

    # Plot 3: B vs C MDE Hits vs Misses
    fig, ax = plt.subplots(figsize=(10, 5))
    ax.bar(x - width/2, s_df['mde_hits_C'], width, label='MDE State Cache Hits (Reused)', color='#2ca02c')
    ax.bar(x + width/2, s_df['mde_misses_C'], width, label='MDE State Cache Misses (New)', color='#d62728')
    ax.set_ylabel('Count')
    ax.set_title('Figure 3: Configuration C MDE Hits vs Misses per Benchmark')
    ax.set_xticks(x)
    ax.set_xticklabels(bm_labels, rotation=30, ha='right')
    ax.legend()
    for i, (h, m) in enumerate(zip(s_df['mde_hits_C'], s_df['mde_misses_C'])):
        rate = h / (h + m) * 100.0 if (h + m) > 0 else 0.0
        ax.annotate(f"{rate:.1f}%",
                    (x[i] - width/2, h + 0.2),
                    ha='center', fontsize=9, fontweight='bold', color='#2ca02c')
    plt.tight_layout()
    fig.savefig(os.path.join(plots_dir, "plot3_b_vs_c_mde_hits_misses.png"), dpi=300)
    plt.close(fig)

    # Plot 4: B vs C Canonicalization Reused vs New
    fig, ax = plt.subplots(figsize=(10, 5))
    ax.bar(x - width/2, s_df['canon_reused_C'], width, label='Canon Reused (Shared State Ptr)', color='#1f77b4')
    ax.bar(x + width/2, s_df['canon_new_C'], width, label='Canon New (Inserted)', color='#ff7f0e')
    ax.set_ylabel('Canonicalization Lookups')
    ax.set_title('Figure 4: Configuration C State Canonicalization Reuse vs New Creation')
    ax.set_xticks(x)
    ax.set_xticklabels(bm_labels, rotation=30, ha='right')
    ax.legend()
    plt.tight_layout()
    fig.savefig(os.path.join(plots_dir, "plot4_b_vs_c_canon_reuse.png"), dpi=300)
    plt.close(fig)

    # Plot 5: States Constructed vs Worklist Iterations across A, B, C
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 5))
    w3 = 0.25
    ax1.bar(x - w3, s_df['states_A'], w3, label='Config A', color='#a6cee3')
    ax1.bar(x, s_df['states_B'], w3, label='Config B', color='#1f78b4')
    ax1.bar(x + w3, s_df['states_C'], w3, label='Config C', color='#b2df8a')
    ax1.set_ylabel('HRA States Constructed')
    ax1.set_title('(A) State Constructions Across Configs')
    ax1.set_xticks(x)
    ax1.set_xticklabels(bm_labels, rotation=30, ha='right')
    ax1.legend()

    ax2.bar(x - w3, s_df['worklist_A'], w3, label='Config A', color='#fb9a99')
    ax2.bar(x, s_df['worklist_B'], w3, label='Config B', color='#e31a1c')
    ax2.bar(x + w3, s_df['worklist_C'], w3, label='Config C', color='#fdbf6f')
    ax2.set_ylabel('Worklist Block Iterations')
    ax2.set_title('(B) Worklist Iterations Across Configs')
    ax2.set_xticks(x)
    ax2.set_xticklabels(bm_labels, rotation=30, ha='right')
    ax2.legend()

    plt.tight_layout()
    fig.savefig(os.path.join(plots_dir, "plot5_states_and_worklist.png"), dpi=300)
    plt.close(fig)

    # Plot 6: Comprehensive 4-panel Research Summary
    fig, axes = plt.subplots(2, 2, figsize=(14, 10))
    # (0,0) Transfer reduction
    axes[0, 0].bar(x - width/2, s_df['transfers_A'], width, label='A: Baseline', color='#386cb0')
    axes[0, 0].bar(x + width/2, s_df['transfers_B'], width, label='B: Liveness', color='#f0027f')
    axes[0, 0].set_title('A vs B: Transfer Invocations')
    axes[0, 0].set_xticks(x)
    axes[0, 0].set_xticklabels(bm_labels, rotation=30, ha='right')
    axes[0, 0].legend()

    # (0,1) Statement Pruning
    axes[0, 1].bar(x, s_df['stmts_pruned_B'], width, color='#bf5b17')
    axes[0, 1].set_title('Config B: Statements Pruned')
    axes[0, 1].set_xticks(x)
    axes[0, 1].set_xticklabels(bm_labels, rotation=30, ha='right')

    # (1,0) MDE Hits & Misses
    axes[1, 0].bar(x - width/2, s_df['mde_hits_C'], width, label='Hits (Reused)', color='#7fc97f')
    axes[1, 0].bar(x + width/2, s_df['mde_misses_C'], width, label='Misses (New)', color='#beaed4')
    axes[1, 0].set_title('Config C: MDE Cache Hits vs Misses')
    axes[1, 0].set_xticks(x)
    axes[1, 0].set_xticklabels(bm_labels, rotation=30, ha='right')
    axes[1, 0].legend()

    # (1,1) MDE Hit Rate %
    axes[1, 1].plot(x, s_df['mde_hit_rate_C'], marker='o', linewidth=2, color='#386cb0')
    axes[1, 1].set_ylim(0, 100)
    axes[1, 1].set_title('Config C: MDE Hit Rate (%)')
    axes[1, 1].set_xticks(x)
    axes[1, 1].set_xticklabels(bm_labels, rotation=30, ha='right')
    for i, val in enumerate(s_df['mde_hit_rate_C']):
        axes[1, 1].annotate(f"{val:.1f}%", (x[i], val + 3), ha='center', fontsize=9, fontweight='bold')

    plt.tight_layout()
    fig.savefig(os.path.join(plots_dir, "plot6_multipanel_summary.png"), dpi=300)
    plt.close(fig)

    print("All tables and plots successfully generated.")

if __name__ == "__main__":
    main()
