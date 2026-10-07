// Native browser editing for the shared canvas fields: IME, touch keyboards,
// selection and clipboard stay under the browser's normal input policy.
class InputFields {
  constructor(container) {
    this.container = container;
    this.fields = new Map();
    this.encoder = new TextEncoder();
  }

  begin(top, height) {
    this.top = top;
    this.container.style.top = `${top}px`;
    this.container.style.height = `${Math.max(0, height)}px`;
    for (const field of this.fields.values()) field.seen = false;
    if (document.title !== 'Ukuvota') document.title = 'Ukuvota';
  }

  field(id, label, placeholder, value, capacity, x, y, width, height) {
    let field = this.fields.get(id);
    if (!field) {
      const input = document.createElement('input');
      input.id = `field-${id}`;
      input.type = 'text';
      input.autocomplete = 'off';
      input.spellcheck = false;
      field = { input, model: value, seen: true, committed: false };
      input.value = value;
      for (const event of [
        'keydown',
        'keyup',
        'keypress',
        'paste',
        'mousedown',
        'mouseup',
        'touchstart',
        'touchmove',
        'touchend',
      ]) {
        input.addEventListener(event, (event) => event.stopPropagation());
      }
      input.addEventListener('keydown', (event) => {
        if (event.key === 'Enter' && !event.isComposing) {
          event.preventDefault();
          field.committed = true;
        }
        if (event.key === 'Escape') input.blur();
      });
      input.addEventListener('input', () => {
        if (this.encoder.encode(input.value).length > field.capacity) {
          let length = 0;
          let limited = '';
          for (const character of input.value) {
            length += this.encoder.encode(character).length;
            if (length > field.capacity) break;
            limited += character;
          }
          input.value = limited;
        }
      });
      this.container.append(input);
      this.fields.set(id, field);
    }
    const { input } = field;
    field.seen = true;
    field.capacity = capacity;
    if (value !== field.model) input.value = value;
    input.setAttribute('aria-label', label);
    input.placeholder = placeholder;
    input.maxLength = capacity;
    Object.assign(input.style, {
      left: `${x}px`,
      top: `${y - this.top}px`,
      width: `${width}px`,
      height: `${height}px`,
    });
    const start = input.selectionStart ?? input.value.length;
    const end = input.selectionEnd ?? start;
    const backwards = input.selectionDirection === 'backward';
    const result = {
      value: input.value,
      cursor: this.encoder.encode(input.value.slice(0, backwards ? start : end)).length,
      anchor: this.encoder.encode(input.value.slice(0, backwards ? end : start)).length,
      focused: document.activeElement === input,
      committed: field.committed,
    };
    field.committed = false;
    field.model = input.value;
    return result;
  }

  end() {
    for (const [id, field] of this.fields) {
      if (!field.seen) {
        field.input.remove();
        this.fields.delete(id);
      }
    }
  }
}
