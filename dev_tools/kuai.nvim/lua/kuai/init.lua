local M = {}
local config = require('kuai.config')

function M.setup(user_opts)
    config.set_options(user_opts)
end

return M
