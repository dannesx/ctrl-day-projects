"""Session aggregates and cached Agg charts. Outcomes are player-relative."""
from collections import Counter
from dataclasses import dataclass


@dataclass(frozen=True)
class Distribution:
    counts: tuple

    @property
    def total(self):
        return sum(self.counts)

    @property
    def percentages(self):
        return tuple(100 * n / self.total for n in self.counts) if self.total else None


def aggregate(jokenpo, tic_tac_toe):
    def distribution(rows, field, categories):
        counts = Counter(row[field] for row in rows)
        return Distribution(tuple(counts[c] for c in categories))
    return (
        distribution(jokenpo, 'player', range(3)),
        distribution(jokenpo, 'computer', range(3)),
        distribution(jokenpo, 'outcome', ('win', 'draw', 'loss')),
        *(distribution([r for r in tic_tac_toe if r['mode'] == mode],
                       'outcome', ('win', 'draw', 'loss')) for mode in ('random', 'trained')),
    )


class SessionCharts:
    """Only redraw changed aggregates; reset the cache when sessions change."""
    def __init__(self):
        self.cache = {}
        self.session = None
        self.render_count = 0

    def images(self, session, jokenpo, tic_tac_toe):
        if session != self.session:
            self.cache.clear()
            self.session = session
        a, b, outcomes, random, trained = aggregate(jokenpo, tic_tac_toe)
        specs = [('choices', (a, b), (360, 354)),
                 ('outcomes', (outcomes,), (250, 354)),
                 ('modes', (random, trained), (620, 354))]
        result = []
        for kind, data, size in specs:
            key = (kind, data, size)
            if self.cache.get(kind, (None,))[0] != key:
                self.cache[kind] = (key, self.render(kind, data, size))
                self.render_count += 1
            result.append(self.cache[kind][1])
        return result

    @staticmethod
    def render(kind, data, size):
        import pygame as pg
        import seaborn as sns
        from matplotlib.figure import Figure
        from matplotlib.backends.backend_agg import FigureCanvasAgg
        from matplotlib.patches import Wedge
        from matplotlib.ticker import PercentFormatter

        bg, ink, muted = '#1c2a3d', '#e7edf6', '#a5b6ca'
        colors = ['#5ce1b7', '#9eb9f5', '#ffbd7a']
        with sns.axes_style('whitegrid'), sns.plotting_context('paper', font_scale=1.05):
            fig = Figure(figsize=(size[0]/100, size[1]/100), dpi=100, facecolor=bg)
            canvas = FigureCanvasAgg(fig)
            title = {'choices': 'Escolhas por jogada', 'outcomes': 'Resultados do Jokenpo',
                     'modes': 'Resultados do jogo da velha'}[kind]
            fig.text(.06, .94, title, color=ink, fontsize=11, weight='bold')
            if kind == 'outcomes':
                dist = data[0]
                ax = fig.add_axes([.08, .37, .84, .45], aspect='equal')
                ax.set(xlim=(-1.1, 1.1), ylim=(-.12, 1.1))
                ax.axis('off')
                ax.add_patch(Wedge((0, 0), 1, 0, 180, width=.23, color='#334257'))
                angle = 180
                if dist.total:
                    for value, color in zip(dist.percentages, colors):
                        end = angle - value * 1.8
                        if value:
                            ax.add_patch(Wedge((0, 0), 1, end, angle, width=.23, facecolor=color, edgecolor=bg))
                        angle = end
                ax.text(0, .27, str(dist.total), color=ink, ha='center', fontsize=27, weight='bold')
                ax.text(0, .06, 'partidas' if dist.total else 'Sem partidas', color=muted, ha='center', fontsize=10)
                for i, (name, count, color) in enumerate(zip(('Vitórias', 'Empates', 'Derrotas'), dist.counts, colors)):
                    suffix = f'{count} · {dist.percentages[i]:.1f}%' if dist.total else '—'
                    fig.text(.09, .29-i*.075, f'●  {name}   {suffix}', color=color, fontsize=10)
            else:
                choices = kind == 'choices'
                names = ('Jogadores', 'ESP') if choices else ('Aleatório', 'Treinado')
                labels = ('Pedra', 'Papel', 'Tesoura') if choices else ('Vitórias', 'Empates', 'Derrotas')
                ax = fig.add_axes([.15 if choices else .10, .20, .81 if choices else .86, .55], facecolor=bg)
                ax.set_ylim(0, 115)
                ax.set_yticks([0, 25, 50, 75, 100])
                ax.yaxis.set_major_formatter(PercentFormatter(100, decimals=0))
                ax.tick_params(colors=muted, labelsize=9)
                ax.grid(axis='y', color='#334257', linewidth=.7)
                ax.grid(axis='x', visible=False)
                ax.set_axisbelow(True)
                for spine in ax.spines.values():
                    spine.set_visible(False)
                for i, (dist, name, color) in enumerate(zip(data, names, (colors[0], colors[2]))):
                    bars = ax.bar([x+(i-.5)*.36 for x in range(3)], dist.percentages or (0, 0, 0), width=.33, color=color, label=name)
                    if dist.total:
                        ax.bar_label(bars, labels=[f'{p:.0f}%' for p in dist.percentages], color=ink, fontsize=9, padding=3)
                    text = f'{name}: {dist.total} partidas' if dist.total else f'{name}: Sem partidas'
                    fig.text(.06, .10-i*.055, text, color=color, fontsize=9)
                ax.set_xticks(range(3), labels, color=ink)
                ax.legend(loc='lower left', bbox_to_anchor=(-.02, 1.02), ncol=2, frameon=False, labelcolor=ink, fontsize=9)
                if not any(d.total for d in data):
                    ax.text(.5, .5, 'Sem partidas', transform=ax.transAxes, ha='center', color=muted, fontsize=13)
            canvas.draw()
            return pg.image.frombuffer(bytes(canvas.buffer_rgba()), size, 'RGBA').copy()
