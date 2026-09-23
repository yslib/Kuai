//! Semantic-analysis diagnostics.

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
    pub fn error(severity: Severity, span: std::ops::Range<usize>, message: String) -> Self {
        Self {
            severity,
            span,
            message,
        }
    }
}
