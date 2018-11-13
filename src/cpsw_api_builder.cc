
#include <cpsw_api_builder.h>
#include <cpsw_compat.h>

void
IVisitor::visit( Dev d )
{
	visit( cpsw::static_pointer_cast<Field::element_type, Dev::element_type>( d ) );
}
