# toolen.bash
_toolen_completion() {
	local cur prev
	COMPREPLY=()
	cur="${COMP_WORDS[COMP_CWORD]}"
	prev="${COMP_WORDS[COMP_CWORD-1]}"
	
	local opts="-l --list -h --help -v --version"

	case "$prev" in
		-h|--help|-v|--version|-l|--list)
			return 0
			;;
	esac

	if [[ "$cur" == -* ]]; then
		COMPREPLY=( $(compgen -W "$opts" -- "$cur") )
		return 0
	fi

	if command -v toolen >/dev/null 2>&1; then
		local modules
		modules=$(toolen --list 2>/dev/null)
		COMPREPLY=( $(compgen -W "$modules" -- "$cur") )
	fi
}

complete -F _toolen_completion toolen
