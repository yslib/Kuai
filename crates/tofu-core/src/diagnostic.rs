#![allow(unused)]
use miette::SourceSpan;
use thiserror::Error;

pub use miette::Report;

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

#[derive(Clone, Debug)]
pub struct Diagnostic {
    pub severity: Severity,
    pub span: std::ops::Range<usize>,
    pub message: String,
}

impl Diagnostic {
    pub fn render_as_miette(&self, filaname: String, src: String) -> MietteDiagnostic {
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
