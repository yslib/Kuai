//! Terminal presentation of driver diagnostics.

use driver::diagnostic::Diagnostic;
use miette::SourceSpan;
use thiserror::Error;

#[derive(Error, Debug, miette::Diagnostic)]
#[error("{message}")]
pub struct MietteDiagnostic {
    pub message: String,
    #[label("Here")]
    pub span: SourceSpan,
    #[source_code]
    pub src: miette::NamedSource<String>,
}

pub fn render_as_miette(
    diagnostic: &Diagnostic,
    filename: String,
    source: String,
) -> MietteDiagnostic {
    MietteDiagnostic {
        message: diagnostic.message.clone(),
        span: SourceSpan::new(
            diagnostic.span.start.into(),
            diagnostic.span.end - diagnostic.span.start,
        ),
        src: miette::NamedSource::new(filename, source),
    }
}

#[cfg(test)]
mod tests {
    use super::render_as_miette;
    use driver::diagnostic::{Diagnostic, Severity};
    use miette::Diagnostic as _;

    #[test]
    fn renderer_preserves_message_label_and_source_location() {
        let input = Diagnostic::error(Severity::Error, 4..5, "unknown name".into());
        let rendered = render_as_miette(&input, "input.ku".into(), "let x = 1;".into());
        assert_eq!(rendered.to_string(), "unknown name");
        assert_eq!(rendered.span.offset(), 4);
        assert_eq!(rendered.span.len(), 1);
        assert_eq!(rendered.src.name(), "input.ku");
        assert_eq!(rendered.src.inner(), "let x = 1;");
        let labels: Vec<_> = rendered.labels().unwrap().collect();
        assert_eq!(labels.len(), 1);
        assert_eq!(labels[0].label(), Some("Here"));
        assert_eq!(labels[0].offset(), 4);
        assert_eq!(labels[0].len(), 1);
    }

    #[test]
    fn renderer_preserves_empty_end_of_input_spans() {
        let input = Diagnostic::error(Severity::Error, 3..3, "missing token".into());
        let rendered = render_as_miette(&input, "stdin".into(), "abc".into());
        assert_eq!(rendered.span.offset(), 3);
        assert_eq!(rendered.span.len(), 0);
    }
}
