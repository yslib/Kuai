use miette::{LabeledSpan, SourceCode, SourceSpan};
use thiserror::Error;

#[derive(Error, Debug)]
#[error("{message}")]
pub struct MietteDiagnostic {
    // 错误消息
    pub message: String,

    // 严重程度映射
    pub severity: Severity,

    // miette 使用 SourceSpan (start, length)
    pub span: SourceSpan,

    // 源代码 (miette 需要它来渲染代码框)
    #[source_code]
    pub src: String,
}

impl miette::Diagnostic for MietteDiagnostic {
    fn code(&self) -> Option<Box<dyn std::fmt::Display>> {
        None
    }

    fn severity(&self) -> Option<miette::Severity> {
        match self.severity {
            Severity::Error => Some(miette::Severity::Error),
            Severity::Warning => Some(miette::Severity::Warning),
            Severity::Hint => Some(miette::Severity::Hint),
        }
    }

    fn labels(&self) -> Option<Box<dyn Iterator<Item = LabeledSpan> + '_>> {}
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
