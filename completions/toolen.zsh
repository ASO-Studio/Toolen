_toolen() {
	local curcontext="$curcontext" state line
	typeset -A opt_args

	_arguments -C \
		'(-l --list)'{-l,--list}'[List all commands]' \
		'(-h --help)'{-h,--help}'[Show help page]' \
		'(-v --version)'{-v,--version}'[Show version]' \
		'*::CommandName:->cmd'

	case $state in
		cmd)
			if (( $+commands[toolen] )); then
				local -a modules
				modules=(${(f)"$(toolen --list 2>/dev/null)"})
				_describe -t modules 'CommandName' cmd
			else
				_message "Cannot find command: toolen"
			fi
			;;
	esac
}

compdef _toolen toolen
