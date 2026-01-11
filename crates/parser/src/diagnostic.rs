use miette::{LabeledSpan, SourceCode, SourceSpan};
use thiserror::Error;

#[derive(Error, Debug, miette::Diagnostic)]
#[error("{message}")]
pub struct MietteDiagnostic {
    // 错误消息
    pub message: String,

    #[label("Here")]
    pub span: SourceSpan,

    #[source_code]
    pub src: miette::NamedSource<String>,
}

#[derive(Debug, Clone)]
pub enum Severity {
    Error,
    Warning,
    Hint,
}

#[derive(Clone)]
pub struct Diagnostic {
    pub severity: Severity,
    pub span: std::ops::Range<usize>,
    pub message: String,
}

impl Diagnostic {
    pub fn render(&self, filaname: String, src: String) -> MietteDiagnostic {
        MietteDiagnostic {
            message: self.message.clone(),
            span: SourceSpan::new(self.span.start.into(), self.span.end - self.span.start),
            src: miette::NamedSource::new(filaname, src),
        }
    }

    pub fn error(severity: Severity, span: std::ops::Range<usize>, message: String) -> Self {
        Self {
            severity,
            span,
            message,
        }
    }
}

impl std::fmt::Display for Diagnostic {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "[{:?}] at {:?}: {}",
            self.severity, self.span, self.message
        )
    }
}
