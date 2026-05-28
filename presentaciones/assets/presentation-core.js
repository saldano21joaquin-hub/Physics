(function () {
  const DEFAULT_OPTIONS = {
    loop: false,
    fit: true,
    minScale: 0.52,
    fitPadding: 28,
    katexDelimiters: [
      { left: '$$', right: '$$', display: true },
      { left: '$', right: '$', display: false },
      { left: '\\(', right: '\\)', display: false },
      { left: '\\[', right: '\\]', display: true }
    ]
  };

  class PresentationRuntime {
    constructor(options) {
      this.options = { ...DEFAULT_OPTIONS, ...(options || {}) };
      this.slides = Array.from(document.querySelectorAll('.slide'));
      this.current = this.findInitialSlide();
      this.mode = this.detectMode();
      this.counterCurrent = document.getElementById('cur') || document.getElementById('current-slide');
      this.counterTotal = document.getElementById('total-slides');
      this.progress = document.getElementById('pb');
      this.resizeFrame = 0;
    }

    start() {
      if (this.slides.length === 0) return;

      this.markDeckElements();
      this.renderMath();
      this.bindEvents();
      this.update({ resetFragments: false });

      if (document.fonts && document.fonts.ready) {
        document.fonts.ready.then(() => this.fitCurrentSlide());
      }
    }

    findInitialSlide() {
      const present = this.slides.findIndex((slide) => slide.classList.contains('present'));
      if (present >= 0) return present;

      const active = this.slides.findIndex((slide) => slide.classList.contains('active'));
      return active >= 0 ? active : 0;
    }

    detectMode() {
      const hasTimelineState = this.slides.some((slide) => (
        slide.classList.contains('present') ||
        slide.classList.contains('past') ||
        slide.classList.contains('future')
      ));

      return hasTimelineState ? 'timeline' : 'active';
    }

    markDeckElements() {
      const counter = this.counterCurrent && this.counterCurrent.closest('.slide-counter');
      if (counter) counter.dataset.deckSync = 'true';
      if (this.progress) this.progress.dataset.deckSync = 'true';
      if (this.counterTotal) this.counterTotal.textContent = this.slides.length;
    }

    renderMath() {
      if (typeof window.renderMathInElement !== 'function') return;

      window.renderMathInElement(document.body, {
        delimiters: this.options.katexDelimiters,
        throwOnError: false
      });

      this.prepareMathForFitting();
    }

    prepareMathForFitting() {
      document.querySelectorAll('.katex-display').forEach((node) => {
        node.classList.add('deck-fit-target');
      });
    }

    bindEvents() {
      document.getElementById('next-btn')?.addEventListener('click', () => this.next());
      document.getElementById('prev-btn')?.addEventListener('click', () => this.prev());

      document.addEventListener('keydown', (event) => {
        if (['ArrowRight', 'ArrowDown', 'PageDown', ' '].includes(event.key)) {
          event.preventDefault();
          this.next();
        }

        if (['ArrowLeft', 'ArrowUp', 'PageUp'].includes(event.key)) {
          event.preventDefault();
          this.prev();
        }

        if (event.key === 'Home') {
          event.preventDefault();
          this.goTo(0);
        }

        if (event.key === 'End') {
          event.preventDefault();
          this.goTo(this.slides.length - 1);
        }
      });

      document.addEventListener('click', (event) => {
        const navButton = event.target.closest('button, a');
        if (navButton) return;

        if (event.target.closest('.slide.present, .slide.active')) {
          this.next();
        }
      });

      window.addEventListener('resize', () => {
        window.cancelAnimationFrame(this.resizeFrame);
        this.resizeFrame = window.requestAnimationFrame(() => this.fitCurrentSlide());
      });
    }

    getFragments(slide) {
      return Array.from(slide.querySelectorAll('.fragment, [data-fragment-index]'))
        .filter((fragment, index, all) => all.indexOf(fragment) === index)
        .sort((a, b) => {
          const aIndex = a.dataset.fragmentIndex !== undefined ? Number.parseInt(a.dataset.fragmentIndex, 10) : 9999;
          const bIndex = b.dataset.fragmentIndex !== undefined ? Number.parseInt(b.dataset.fragmentIndex, 10) : 9999;
          return aIndex - bIndex;
        });
    }

    next() {
      const slide = this.slides[this.current];
      const nextFragment = this.getFragments(slide).find((fragment) => !fragment.classList.contains('visible'));

      if (nextFragment) {
        const index = nextFragment.dataset.fragmentIndex;
        if (index !== undefined) {
          this.getFragments(slide)
            .filter((fragment) => fragment.dataset.fragmentIndex === index)
            .forEach((fragment) => fragment.classList.add('visible'));
        } else {
          nextFragment.classList.add('visible');
        }

        this.fitCurrentSlide();
        return;
      }

      if (this.current < this.slides.length - 1) {
        this.goTo(this.current + 1);
      } else if (this.options.loop) {
        this.goTo(0);
      }
    }

    prev() {
      const slide = this.slides[this.current];
      const visibleFragments = this.getFragments(slide).filter((fragment) => fragment.classList.contains('visible'));

      if (visibleFragments.length > 0) {
        const last = visibleFragments[visibleFragments.length - 1];
        const index = last.dataset.fragmentIndex;
        const toHide = index !== undefined
          ? visibleFragments.filter((fragment) => fragment.dataset.fragmentIndex === index)
          : [last];

        toHide.forEach((fragment) => fragment.classList.remove('visible'));
        this.fitCurrentSlide();
        return;
      }

      if (this.current > 0) {
        this.goTo(this.current - 1, { revealFragments: true });
      } else if (this.options.loop) {
        this.goTo(this.slides.length - 1, { revealFragments: true });
      }
    }

    goTo(index, options) {
      const nextIndex = Math.max(0, Math.min(index, this.slides.length - 1));
      this.current = nextIndex;
      this.update({ resetFragments: true, ...(options || {}) });
    }

    update(options) {
      this.slides.forEach((slide, index) => {
        slide.classList.remove('past', 'present', 'future', 'active');

        if (this.mode === 'timeline') {
          if (index < this.current) slide.classList.add('past');
          else if (index === this.current) slide.classList.add('present');
          else slide.classList.add('future');
        } else if (index === this.current) {
          slide.classList.add('active');
        }

        if (options.resetFragments && index === this.current) {
          this.getFragments(slide).forEach((fragment) => fragment.classList.remove('visible'));
        }
      });

      if (options.revealFragments) {
        this.getFragments(this.slides[this.current]).forEach((fragment) => fragment.classList.add('visible'));
      }

      this.updateCounters();
      this.fitCurrentSlide();
    }

    updateCounters() {
      if (this.counterCurrent) this.counterCurrent.textContent = this.current + 1;
      if (this.counterTotal) this.counterTotal.textContent = this.slides.length;

      if (this.progress) {
        const divisor = Math.max(1, this.slides.length - 1);
        this.progress.style.width = `${(this.current / divisor) * 100}%`;
      }
    }

    fitCurrentSlide() {
      if (!this.options.fit) return;

      const slide = this.slides[this.current];
      if (!slide) return;

      this.fitBlockMath(slide);
      this.fitSlideContent(slide);
    }

    fitBlockMath(slide) {
      slide.querySelectorAll('.deck-fit-target').forEach((node) => {
        node.style.setProperty('--deck-equation-scale', '1');
        const parentWidth = Math.max(1, node.parentElement ? node.parentElement.clientWidth : slide.clientWidth);
        const overflowRatio = node.scrollWidth / parentWidth;
        const scale = overflowRatio > 1 ? Math.max(0.62, 1 / overflowRatio) : 1;
        node.style.setProperty('--deck-equation-scale', scale.toFixed(3));
      });
    }

    fitSlideContent(slide) {
      const content = slide.querySelector(':scope > .inner, :scope > .slide-content');
      if (!content) return;

      slide.classList.remove('deck-overflow-risk');
      slide.style.setProperty('--deck-content-scale', '1');

      const slideStyle = window.getComputedStyle(slide);
      const horizontalPadding = Number.parseFloat(slideStyle.paddingLeft) + Number.parseFloat(slideStyle.paddingRight);
      const verticalPadding = Number.parseFloat(slideStyle.paddingTop) + Number.parseFloat(slideStyle.paddingBottom);
      const availableWidth = Math.max(1, slide.clientWidth - horizontalPadding - this.options.fitPadding);
      const availableHeight = Math.max(1, slide.clientHeight - verticalPadding - this.options.fitPadding);

      const contentWidth = Math.max(content.scrollWidth, content.getBoundingClientRect().width);
      const contentHeight = Math.max(content.scrollHeight, content.getBoundingClientRect().height);
      const widthScale = availableWidth / Math.max(1, contentWidth);
      const heightScale = availableHeight / Math.max(1, contentHeight);
      const scale = Math.max(this.options.minScale, Math.min(1, widthScale, heightScale));

      if (scale < 0.995) {
        slide.classList.add('deck-overflow-risk');
        slide.style.setProperty('--deck-content-scale', scale.toFixed(3));
      }
    }
  }

  function start(options) {
    window.deckRuntime = new PresentationRuntime(options);
    window.deckRuntime.start();
  }

  window.PresentationRuntime = PresentationRuntime;
  window.initPresentation = function initPresentation(options) {
    start(options);
  };

  window.next = function next() {
    if (window.deckRuntime) window.deckRuntime.next();
  };

  window.prev = function prev() {
    if (window.deckRuntime) window.deckRuntime.prev();
  };

  window.addEventListener('DOMContentLoaded', () => {
    if (!window.deckRuntime) start(window.PRESENTATION_OPTIONS || {});
  });
}());
