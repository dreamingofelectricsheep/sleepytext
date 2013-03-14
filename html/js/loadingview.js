function enterloadingview()
{
	view = {}
	

	view.loading = tags.div({ class: 'loading' }, 'Loading')
	body.appendChild(view.loading)



	view.leave = function()
	{
		body.removeChild(view.loading)
	}
}
